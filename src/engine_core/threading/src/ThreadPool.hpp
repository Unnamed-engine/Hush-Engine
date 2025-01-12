/*! \file ThreadPool.hpp
    \author Alan Ramirez
    \date 2024-12-25
    \brief ThreadPool implementation
*/

#pragma once

#include "async/SyncWait.hpp"
#include "async/Task.hpp"

#include <array>
#include <chrono>
#include <coroutine>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

namespace Hush::Threading
{
    class ThreadPool;
    class TaskOperation;

    namespace impl
    {
        /// WorkerQueue is a queue of tasks that a worker thread will execute.
        /// This is a lock-free queue.
        class WorkerQueue
        {
            /// The size of the worker queue. This is fixed.
            constexpr static std::size_t WORKER_QUEUE_SIZE = 256;

        public:
            /// Constructs a new worker queue.
            WorkerQueue(ThreadPool *threadPool);

            /// Move constructor.
            /// @param other Other WorkerQueue to move from.
            WorkerQueue(WorkerQueue &&other) noexcept;

            /// Move assignment operator.
            /// @param other Other WorkerQueue to move from.
            /// @return self
            WorkerQueue &operator=(WorkerQueue &&other) noexcept;

            /// Pushes a task to the queue.
            /// @param task The task to push to the queue.
            void Push(TaskOperation *task);

            /// Pops a task from the queue.
            /// @return A task from the queue. nullptr if the queue is empty.
            TaskOperation *Pop();

            /// Steals a task from the queue.
            /// @return A task from the queue. nullptr if the queue is empty.
            TaskOperation *Steal();

            /// Steals a number of tasks from the global queue.
            /// @return The number of stolen tasks.
            [[nodiscard]]
            std::size_t TakeFromGlobalQueue();

        private:
            /// Reference to the thread pool, it is used to push tasks to the global queue in case the worker queue is
            /// full.
            ThreadPool *m_threadPool;

            /// The worker queue
            std::array<TaskOperation *, WORKER_QUEUE_SIZE> m_tasks;

            /// Bottom of the queue
            std::atomic<std::int64_t> m_bottom;

            /// Top of the queue
            std::atomic<std::int64_t> m_top;
        };

        class WorkerThread
        {
        public:
            // Default number of tasks to steal from the global queue.
            constexpr static std::uint32_t DEFAULT_STEAL_COUNT = 10;

            /// Options for the worker thread.
            struct ThreadOptions
            {
                /// The affinity of the worker thread, if 0, the worker thread will have no affinity.
                std::int32_t affinity = 0;
            };

            /// Enum class that represents the state of the worker thread.
            enum class EWorkerThreadState
            {
                None,
                Starting,
                Running,
                Idle,
                Stopping,
                Stopped
            };

            /// Enum class that represents the stop mode of the worker thread.
            enum class EStopMode : bool
            {
                FinishPendingTasks,
                StopImmediately
            };

            /// Constructs a new worker thread.
            /// @param threadPool The thread pool that the worker thread belongs to.
            /// @param threadIndex The index of the worker thread.
            /// @param options The options for the worker thread.
            WorkerThread(std::unique_ptr<WorkerQueue> workerQueue,
                         std::uint32_t threadIndex,
                         ThreadOptions options) noexcept;

            WorkerThread(const WorkerThread &) = delete;
            WorkerThread &operator=(const WorkerThread &) = delete;

            ~WorkerThread();

            /// Starts the worker thread.
            void Start();

            /// Gets the state of the worker thread.
            EWorkerThreadState GetState() const noexcept
            {
                return m_state;
            }

            /// Stops the worker thread.
            /// @param stopMode Stop mode to use.
            void Stop(EStopMode stopMode);

            /// Gets the options for the worker thread.
            /// @return The options for the worker thread.
            [[nodiscard]]
            ThreadOptions GetOptions() const noexcept
            {
                return m_options;
            }

            /// Notifies the worker thread.
            void Notify();

        private:
            /// The main function of the worker thread.
            /// @param stopToken Stop token for the worker thread.
            void ThreadFunction(std::stop_token stopToken);

            /// The worker queue for the worker thread.
            std::unique_ptr<WorkerQueue> m_workerQueue;
            std::jthread m_thread;
            std::uint32_t m_threadIndex;
            ThreadOptions m_options;
            std::atomic<EWorkerThreadState> m_state = EWorkerThreadState::None;
            EStopMode m_stopMode = EStopMode::FinishPendingTasks;
        };
    }; // namespace impl

    struct Job : public SyncWaitTask<void>
    {
        using SyncWaitTask<void>::SyncWaitTask;
        using SyncWaitTask<void>::operator=;
        using SyncWaitTask<void>::promise;
        using SyncWaitTask<void>::GetCoroutine;

        void Wait()
        {
            promise().Wait();
        }

    private:
        friend class ThreadPool;

        void Start(std::atomic_flag &done)
        {
            promise().Start(done);
        }
    };

    class TaskOperation
    {
        friend class ThreadPool;
        friend class impl::WorkerThread;

        explicit TaskOperation(ThreadPool &executor,
                               std::int32_t threadAffinity = -1,
                               bool shouldDeleteWhenDone = false)
            : m_executor(executor),
              m_threadAffinity(threadAffinity),
              m_shouldDeleteWhenDone(shouldDeleteWhenDone)
        {
        }

    public:
        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<Job::promise_type> awaitingCoroutine) noexcept;

        void await_resume() noexcept;

    private:
        ThreadPool &m_executor;
        std::atomic_flag m_done;
        std::coroutine_handle<> m_awaitingCoroutine = nullptr;
        std::int32_t m_threadAffinity = -1;
        bool m_shouldDeleteWhenDone = false;
    };

    class ThreadPool
    {

    public:
        /// Constructs a new thread pool.
        /// @param numThreads The number of threads in the thread pool.
        ThreadPool(std::uint32_t numThreads);

        /// Destroys the thread pool.
        ~ThreadPool();

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;

        ThreadPool(ThreadPool &&) = delete;
        ThreadPool &operator=(ThreadPool &&) = delete;

        /// Schedules the current task.
        /// @return TaskOperation that schedules the current task.
        TaskOperation ScheduleCurrentTask();

        /// Schedules a task.
        /// @param task The task to schedule.
        /// @return Task wrapped that will be executed by the thread pool.
        Job ScheduleTask(Task<void> &&task);

        /// Schedules a function.
        /// @tparam Fn The function type.
        /// @tparam Args The arguments type.
        /// @param function Function to schedule.
        /// @param args Arguments to pass to the function.
        /// @return Task wrapped that will be executed by the thread pool.
        template <typename Fn, typename... Args>
        requires !Concepts::Awaitable<std::invoke_result_t<Fn, Args...>>
        Job ScheduleFunction(Fn &&function, Args &&...args)
        {
            auto wrapper = [this](Fn &&function, Args &&...args) -> Job {
                co_await ScheduleCurrentTask();
                std::invoke(std::forward<Fn>(function), std::forward<Args>(args)...);
                co_return;
            };

            Job task = wrapper(std::forward<Fn>(function), std::forward<Args>(args)...);

            task.GetCoroutine().resume();

            return std::move(task);
        }

        /// Starts the thread pool.
        void Start();

        /// Waits until all tasks are done.
        /// TODO: should we remove this function?, i.e. implement a way to wait for a specific task?, something like
        /// Hush::SyncWait(task)
        void WaitUntilDone();

        /// @return The number of threads in the thread pool.
        [[nodiscard]]
        std::uint32_t GetNumThreads() const noexcept
        {
            return static_cast<std::uint32_t>(m_workerThreads.size());
        }

    private:
        /// Steals a task from the global queue.
        /// @return A task from the global queue.
        TaskOperation *StealFromGlobalQueue();

        /// Notifies the worker threads.
        /// @param tasks Span that will contain the tasks that were stolen.
        /// @return The number of tasks that were stolen.
        std::size_t StealFromGlobalQueue(std::span<TaskOperation *> tasks);

        static Job WrapTask(ThreadPool &threadPool, Task<void> function);

        /// Notifies the worker threads.
        void NotifyWorkerThreads();

        /// Pushes a task to the global queue.
        /// @param task The task to push to the global queue.
        void PushToGlobalQueue(TaskOperation *task);

        friend class impl::WorkerQueue;
        friend class TaskOperation;

        std::vector<std::unique_ptr<impl::WorkerThread>> m_workerThreads;
        std::vector<TaskOperation *> m_globalQueue;
        std::mutex m_globalQueueMutex;
    };

    inline void Wait(Job &job)
    {
        job.Wait();
    }
} // namespace Hush::Threading