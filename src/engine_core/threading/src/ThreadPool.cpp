/*! \file ThreadPool.cpp
    \author Alan Ramirez
    \date 2024-12-25
    \brief ThreadPool implementation
*/
#include "ThreadPool.hpp"
#include "Platform.hpp"
#include "async/Task.hpp"

#include <Logger.hpp>
#include <mutex>
#include <semaphore>
#include <thread>

Hush::Threading::impl::WorkerQueue::WorkerQueue(ThreadPool *threadPool)
    : m_threadPool(threadPool),
      m_tasks(),
      m_bottom(0),
      m_top(0)
{
}

Hush::Threading::impl::WorkerQueue::WorkerQueue(WorkerQueue &&other) noexcept
    : m_threadPool(other.m_threadPool),
      m_bottom(other.m_bottom.load(std::memory_order_acquire)),
      m_top(other.m_top.load(std::memory_order_acquire)),
      m_tasks(std::move(other.m_tasks))
{
    // We use release memory order since we are moving the tasks to another queue.
    other.m_bottom.store(0, std::memory_order_release);
    other.m_top.store(0, std::memory_order_release);
}

Hush::Threading::impl::WorkerQueue &Hush::Threading::impl::WorkerQueue::operator=(WorkerQueue &&other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    this->m_threadPool = std::move(other.m_threadPool);
    this->m_bottom = other.m_bottom.load(std::memory_order_acquire);
    this->m_top = other.m_top.load(std::memory_order_acquire);
    this->m_tasks = std::move(other.m_tasks);

    other.m_bottom.store(0, std::memory_order_release);
    other.m_top.store(0, std::memory_order_release);
    other.m_threadPool = nullptr;

    return *this;
}

void Hush::Threading::impl::WorkerQueue::Push(TaskOperation *task)
{
    // Push a task to the bottom of the queue
    std::int64_t bottom = m_bottom.load(std::memory_order_acquire);
    std::int64_t newBottom = bottom + 1;

    if (newBottom >= WORKER_QUEUE_SIZE)
    {
        // The queue is full
        // Push the task to the global queue
        m_threadPool->PushToGlobalQueue(task);
        return;
    }

    m_tasks[bottom % WORKER_QUEUE_SIZE] = task;

    // Ok, just update the bottom
    m_bottom.store(newBottom, std::memory_order_release);
}

Hush::Threading::TaskOperation *Hush::Threading::impl::WorkerQueue::Pop()
{
    // Pops a task from the bottom of the queue.
    m_bottom.fetch_sub(1, std::memory_order_acquire);
    std::int64_t bottom = m_bottom.load(std::memory_order_acquire);
    std::int64_t top = m_top.load(std::memory_order_acquire);

    if (top > bottom)
    {
        // The queue is empty
        m_bottom.store(top, std::memory_order_release);
        return nullptr;
    }

    // Non-empty queue, just pop the task
    TaskOperation *task = m_tasks[bottom];

    if (top != bottom)
    {
        // There are more tasks in the queue.
        return task;
    }

    // Last task in the queue
    if (!m_top.compare_exchange_strong(top, (top + 1) % WORKER_QUEUE_SIZE))
    {
        // Another thread stole the task
        return nullptr;
    }

    m_bottom.store(top + 1, std::memory_order_release);

    return task;
}

Hush::Threading::TaskOperation *Hush::Threading::impl::WorkerQueue::Steal()
{
    // Steals a task from the top of the queue.
    std::int64_t top = m_top.load(std::memory_order_acquire);
    std::int64_t bottom = m_bottom.load(std::memory_order_acquire);

    // Check if the queue is empty
    if (top >= bottom)
    {
        return nullptr;
    }

    // Non-empty queue, just steal the task
    TaskOperation *task = m_tasks[top % WORKER_QUEUE_SIZE];

    if (!m_top.compare_exchange_strong(top, (top + 1) % WORKER_QUEUE_SIZE))
    {
        // Another thread stole the task
        return nullptr;
    }

    // Task stolen
    return task;
}

std::size_t Hush::Threading::impl::WorkerQueue::TakeFromGlobalQueue()
{
    std::array<TaskOperation *, WorkerThread::DEFAULT_STEAL_COUNT> tasks;

    const auto tasksAcquired = m_threadPool->StealFromGlobalQueue(tasks);

    for (std::size_t i = 0; i < tasksAcquired; ++i)
    {
        Push(tasks[i]);
    }

    return tasksAcquired;
}

#if HUSH_PLATFORM_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#endif

static void SetCurrentThreadAffinity(std::uint32_t affinity)
{
#if HUSH_PLATFORM_WIN
    const DWORD_PTR mask = 1ULL << static_cast<std::uint64_t>(affinity);
    SetThreadAffinityMask(GetCurrentThread(), mask);
#endif
}

Hush::Threading::impl::WorkerThread::WorkerThread(std::unique_ptr<WorkerQueue> workerQueue,
                                                  std::uint32_t threadIndex,
                                                  ThreadOptions options) noexcept
    : m_workerQueue(std::move(workerQueue)),
      m_threadIndex(threadIndex),
      m_options(options)
{
    m_thread = std::jthread([this](std::stop_token stopToken) {
        if (m_options.affinity >= 0)
        {
            SetCurrentThreadAffinity(m_options.affinity);
        }

        // Wait until the thread is started
        m_state.wait(EWorkerThreadState::Starting, std::memory_order_acquire);

        // Start the thread function
        this->ThreadFunction(stopToken);
    });
}

Hush::Threading::impl::WorkerThread::~WorkerThread()
{
}

void Hush::Threading::impl::WorkerThread::Start()
{
    // Notify the thread to start with the condition variable
    m_state.store(EWorkerThreadState::Starting, std::memory_order_release);
    m_state.notify_all();
}

void Hush::Threading::impl::WorkerThread::Stop(EStopMode stopMode)
{
    m_stopMode = stopMode;

    // Stop the thread
    m_thread.request_stop();

    // Ensure to wake up the thread
    m_state.store(EWorkerThreadState::Stopping, std::memory_order_release);
    m_state.notify_all();
}

void Hush::Threading::impl::WorkerThread::Notify()
{
    m_state.store(EWorkerThreadState::Running, std::memory_order_release);
    m_state.notify_all();
}

void Hush::Threading::impl::WorkerThread::ThreadFunction(std::stop_token stopToken)
{
    m_state = EWorkerThreadState::Running;

    while (!stopToken.stop_requested())
    {
        TaskOperation *task = m_workerQueue->Pop();

        if (task == nullptr)
        {
            // Steal a task
            task = m_workerQueue->Steal();
        }

        // No task could be stolen, check the global queue
        std::size_t acquiredTasks = 0;
        if (task == nullptr)
        {
            acquiredTasks = m_workerQueue->TakeFromGlobalQueue();
        }

        if (task != nullptr)
        {
            task->m_awaitingCoroutine.resume();

            if (task->m_shouldDeleteWhenDone)
            {
                task->m_awaitingCoroutine.destroy();
            }
        }
        else if (acquiredTasks == 0)
        {
            // No task could be found either in the local, external local, or global queue.
            // Perform some busy waiting
            // TODO: Perform some busy waiting while checking for tasks in the current thread's queue
            std::this_thread::sleep_for(std::chrono::milliseconds(1));


            // It might be that the thread was stopped after the while loop condition was checked, so we need to check again before going to sleep
            if (m_state.load(std::memory_order_acquire) == EWorkerThreadState::Running)
            {
                // No tasks to execute, put the thread to sleep
                m_state.store(EWorkerThreadState::Idle);
                m_state.wait(EWorkerThreadState::Idle, std::memory_order_acquire);
                // Thread woke up, continue execution
            }


        }
    }

    if (m_stopMode == EStopMode::FinishPendingTasks)
    {
        // Finish pending tasks
        TaskOperation *task = m_workerQueue->Pop();

        while (task != nullptr)
        {
            task->m_awaitingCoroutine.resume();
            task = m_workerQueue->Pop();
        }
    }
    m_state.store(EWorkerThreadState::Stopped, std::memory_order_relaxed);
}

void Hush::Threading::TaskOperation::await_suspend(std::coroutine_handle<Job::promise_type> awaitingCoroutine) noexcept
{
    m_awaitingCoroutine = awaitingCoroutine;
    m_executor.PushToGlobalQueue(this);

    awaitingCoroutine.promise().SetFlag(m_done);

    m_executor.NotifyWorkerThreads();
}

void Hush::Threading::TaskOperation::await_resume() noexcept
{
}

Hush::Threading::ThreadPool::ThreadPool(std::uint32_t numThreads)
{
    const std::uint32_t numHardwareThreads = std::thread::hardware_concurrency();
    // Get hardware threads
    if (numThreads == 0)
    {
        numThreads = std::thread::hardware_concurrency();
    }

    for (std::uint32_t i = 0; i < numThreads; ++i)
    {
        const std::int32_t affinity = numThreads == numHardwareThreads ? static_cast<std::int32_t>(i) : -1;
        auto workerQueue = std::make_unique<impl::WorkerQueue>(this);

        const auto threadOptions = impl::WorkerThread::ThreadOptions{.affinity = affinity};

        m_workerThreads.emplace_back(std::make_unique<impl::WorkerThread>(std::move(workerQueue), i, threadOptions));
    }
}
Hush::Threading::ThreadPool::~ThreadPool()
{
    for (auto &thread : m_workerThreads)
    {
        thread->Stop(impl::WorkerThread::EStopMode::StopImmediately);
    }
}
Hush::Threading::TaskOperation Hush::Threading::ThreadPool::ScheduleCurrentTask()
{
    return TaskOperation(*this);
}

Hush::Threading::Job Hush::Threading::ThreadPool::WrapTask(ThreadPool &threadPool, Task<void> function)
{
    co_await threadPool.ScheduleCurrentTask();
    co_await function;
}

Hush::Threading::Job Hush::Threading::ThreadPool::ScheduleTask(Task<void> &&task)
{
    auto wrapper = WrapTask(*this, std::move(task));

    wrapper.GetCoroutine().resume();

    return std::move(wrapper);
}

void Hush::Threading::ThreadPool::Start()
{
    // Start the threads
    for (auto &thread : m_workerThreads)
    {
        thread->Start();
    }
}

void Hush::Threading::ThreadPool::WaitUntilDone()
{
    // Wait until all threads are Idle AND the global queue is empty
    bool allThreadsIdle = false;
    bool globalQueueEmpty = false;

    while (!allThreadsIdle || !globalQueueEmpty)
    {
        allThreadsIdle = true;

        {
            std::lock_guard lock(m_globalQueueMutex);
            globalQueueEmpty = m_globalQueue.empty();
        }

        if (!globalQueueEmpty)
        {
            for (auto &thread : m_workerThreads)
            {
                // Check if thread is not idle or stopped
                if (thread->GetState() != impl::WorkerThread::EWorkerThreadState::Idle &&
                    thread->GetState() != impl::WorkerThread::EWorkerThreadState::Stopped)
                {
                    allThreadsIdle = false;
                    break;
                }
            }

            // For each idle thread, notify that the global queue is not empty
            for (auto &thread : m_workerThreads)
            {
                if (thread->GetState() == impl::WorkerThread::EWorkerThreadState::Idle)
                {
                    thread->Notify();
                }
            }
        }

        if (!allThreadsIdle || !globalQueueEmpty)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

Hush::Threading::TaskOperation *Hush::Threading::ThreadPool::StealFromGlobalQueue()
{
    // Lock the global queue
    std::lock_guard lock(m_globalQueueMutex);

    if (m_globalQueue.empty())
    {
        return nullptr;
    }

    TaskOperation *task = m_globalQueue.back();
    m_globalQueue.pop_back();

    return task;
}

std::size_t Hush::Threading::ThreadPool::StealFromGlobalQueue(std::span<TaskOperation *> tasks)
{
    std::size_t stolenTasks = 0;

    std::lock_guard lock(m_globalQueueMutex);
    for (std::size_t i = 0; i < tasks.size(); ++i)
    {
        if (m_globalQueue.empty())
        {
            break;
        }

        tasks[i] = m_globalQueue.back();
        m_globalQueue.pop_back();
        ++stolenTasks;
    }

    return stolenTasks;
}

void Hush::Threading::ThreadPool::NotifyWorkerThreads()
{
    for (auto &thread : m_workerThreads)
    {
        thread->Notify();
    }
}

void Hush::Threading::ThreadPool::PushToGlobalQueue(TaskOperation *task)
{
    // Lock the global queue
    std::lock_guard lock(m_globalQueueMutex);
    m_globalQueue.push_back(task);
}
