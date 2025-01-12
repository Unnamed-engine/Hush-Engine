/*! \file SyncWait.hpp
    \author Alan Ramirez
    \date 2025-01-07
    \brief Sync wait task
*/

#pragma once

#include "TaskTraits.hpp"
#include <atomic>
#include <coroutine>

namespace Hush::Threading
{
    template <typename T>
    class SyncWait;

    namespace impl
    {
        struct SyncWaitPromiseBase
        {
            SyncWaitPromiseBase() noexcept = default;

            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            void SetFlag(std::atomic_flag &done) noexcept
            {
                m_done = &done;
            }

        protected:
            ~SyncWaitPromiseBase() = default;

        protected:
            std::atomic_flag *m_done = nullptr;
        };

        template <typename T>
        struct SyncWaitPromise : public SyncWaitPromiseBase
        {
            using ResultType =
                std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T> *, std::remove_const_t<T>>;

            using VariantType = std::variant<std::monostate, ResultType, std::exception_ptr>;
            using CoroutineType = std::coroutine_handle<SyncWaitPromise>;

            SyncWait<T> get_return_object() noexcept
            {
                return CoroutineType::from_promise(*this);
            }

            SyncWaitPromise() noexcept = default;

            SyncWaitPromise(SyncWaitPromise &&) noexcept = delete;
            SyncWaitPromise &operator=(SyncWaitPromise &&) noexcept = delete;

            SyncWaitPromise(const SyncWaitPromise &) noexcept = delete;
            SyncWaitPromise &operator=(const SyncWaitPromise &) noexcept = delete;

            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            void Start(std::atomic_flag &done) noexcept
            {
                m_done = &done;
                auto coroutinePromise = CoroutineType::from_promise(*this);
                coroutinePromise.resume();
            }

            void Wait() noexcept
            {
                m_done->wait(true, std::memory_order_relaxed);
            }

            template <typename U>
                requires(std::is_reference_v<T> && std::is_constructible_v<ResultType, U &&> ||
                         (!std::is_reference_v<T> && std::is_constructible_v<ResultType, U>))
            auto return_value(U &&value) noexcept
            {
                if constexpr (std::is_reference_v<T>)
                {
                    m_result.template emplace<ResultType>(std::addressof(value));
                }
                else
                {
                    m_result.template emplace<ResultType>(std::forward<U>(value));
                }
            }

            auto return_value(ResultType value) noexcept
            {
                if constexpr (std::is_move_constructible_v<ResultType>)
                {
                    m_result.template emplace<ResultType>(std::move(value));
                }
                else
                {
                    m_result.template emplace<ResultType>(value);
                }
            }

            auto final_suspend() noexcept
            {
                struct CompletionAwaiter
                {
                    bool await_ready() const noexcept
                    {
                        return false;
                    }

                    void await_suspend(std::coroutine_handle<SyncWaitPromise> coroutine) noexcept
                    {
                        coroutine.promise().m_done->test_and_set(std::memory_order_relaxed);
                        coroutine.promise().m_done->notify_one();
                    }

                    void await_resume() noexcept
                    {
                    }
                };

                return CompletionAwaiter{};
            }

            decltype(auto) Result() & noexcept
            {
                if (std::holds_alternative<ResultType>(m_result))
                {
                    if constexpr (std::is_reference_v<T>)
                    {
                        return static_cast<T>(*std::get<ResultType>(m_result));
                    }
                    else
                    {
                        return static_cast<const T &>(*std::get<ResultType>(m_result));
                    }
                }
                if (std::holds_alternative<std::exception_ptr>(m_result))
                {
                    std::rethrow_exception(std::get<std::exception_ptr>(m_result));
                }
                assert(false);
            }

            decltype(auto) Result() const & noexcept
            {
                if (std::holds_alternative<ResultType>(m_result))
                {
                    if constexpr (std::is_reference_v<T>)
                    {
                        return static_cast<std::add_const_t<T>>(*std::get<ResultType>(m_result));
                    }
                    else
                    {
                        return static_cast<const T &>(*std::get<ResultType>(m_result));
                    }
                }
                if (std::holds_alternative<std::exception_ptr>(m_result))
                {
                    std::rethrow_exception(std::get<std::exception_ptr>(m_result));
                }
                assert(false);
            }

            decltype(auto) Result() && noexcept
            {
                if (std::holds_alternative<ResultType>(m_result))
                {
                    if constexpr (std::is_reference_v<T>)
                    {
                        return static_cast<T>(*std::get<ResultType>(m_result));
                    }
                    else if constexpr (std::is_assignable_v<T, ResultType>)
                    {
                        return static_cast<T &&>(std::get<ResultType>(m_result));
                    }
                    else
                    {
                        return static_cast<const T &&>(std::get<ResultType>(m_result));
                    }
                }
                if (std::holds_alternative<std::exception_ptr>(m_result))
                {
                    std::rethrow_exception(std::get<std::exception_ptr>(m_result));
                }
                assert(false);
            }

            void unhandled_exception() noexcept
            {
                m_result.template emplace<std::exception_ptr>(std::current_exception());
            }

        private:
            VariantType m_result;
        };

        template <>
        struct SyncWaitPromise<void> final : public SyncWaitPromiseBase
        {
            using CoroutineType = std::coroutine_handle<SyncWaitPromise>;

            SyncWaitPromise() noexcept = default;
            ~SyncWaitPromise() = default;

            void Start(std::atomic_flag &done) noexcept
            {
                m_done = &done;
                auto coroutinePromise = CoroutineType::from_promise(*this);
                coroutinePromise.resume();
            }

            void Wait() noexcept
            {
                m_done->wait(false, std::memory_order_relaxed);
            }

            auto get_return_object() noexcept
            {
                return CoroutineType::from_promise(*this);
            }

            auto final_suspend() noexcept
            {
                struct CompletionAwaiter
                {
                    bool await_ready() const noexcept
                    {
                        return false;
                    }

                    void await_suspend(std::coroutine_handle<SyncWaitPromise> coroutine) noexcept
                    {
                        std::atomic_flag* done = coroutine.promise().m_done;
                        done->test_and_set(std::memory_order_relaxed);
                        done->notify_one();
                    }

                    void await_resume() noexcept
                    {
                    }
                };

                return CompletionAwaiter{};
            }

            void return_void() noexcept
            {
            }

            void Result() noexcept
            {
                if (m_exception != nullptr)
                {
                    std::rethrow_exception(m_exception);
                }
            }

            void unhandled_exception() noexcept
            {
                m_exception = std::current_exception();
            }

        private:
            std::exception_ptr m_exception;
        };
    } // namespace impl

    /// Sync wait task
    template <typename T>
    class SyncWaitTask
    {
    public:
        using promise_type = impl::SyncWaitPromise<T>;
        using CoroutineType = std::coroutine_handle<promise_type>;

        /// Constructor
        /// @param coroutine coroutine handle
        SyncWaitTask(CoroutineType coroutine) noexcept
            : m_coroutine(coroutine)
        {
        }

        /// Move constructor
        /// @param other Other SyncWaitTask to move from
        SyncWaitTask(SyncWaitTask &&other) noexcept
            : m_coroutine(other.m_coroutine)
        {
            other.m_coroutine = nullptr;
        }

        /// Move assignment operator
        /// @param other Other SyncWaitTask to move from
        /// @return self
        SyncWaitTask &operator=(SyncWaitTask &&other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            m_coroutine = other.m_coroutine;
            other.m_coroutine = nullptr;

            return *this;
        }

        /// Destructor
        ~SyncWaitTask()
        {
            if (m_coroutine)
            {
                m_coroutine.destroy();
            }
        }

        /// Returns the promise
        /// @return The promise
        promise_type &promise() & noexcept
        {
            return m_coroutine.promise();
        }

        /// Returns the promise
        /// @return The promise
        const promise_type &promise() const & noexcept
        {
            return m_coroutine.promise();
        }

        /// Returns the promise
        /// @return The promise
        promise_type &&promise() && noexcept
        {
            return std::move(m_coroutine.promise());
        }

        /// Returns the coroutine
        /// @return The coroutine
        CoroutineType GetCoroutine() const noexcept
        {
            return m_coroutine;
        }

        /// Returns the coroutine
        /// @return The coroutine
        CoroutineType GetCoroutine() noexcept
        {
            return m_coroutine;
        }

        /// Returns the result. If the coroutine is not done, it will crash.
        /// @return Get the result
        T Result() noexcept;

    protected:
        CoroutineType &GetCoroutineRef() noexcept
        {
            return m_coroutine;
        }

    private:
        CoroutineType m_coroutine;
    };

    namespace impl
    {
        template <Concepts::Awaitable A, typename T>
        static SyncWaitTask<T> MakeSyncWaitTask(A &&awaitable)
        {
            if constexpr (std::is_void_v<T>)
            {
                co_await std::forward<A>(awaitable);
                co_return;
            }
            else
            {
                co_return co_await std::forward<A>(awaitable);
            }
        }
    } // namespace impl

    template <Concepts::Awaitable A, typename T = typename Concepts::AwaitableTraits<A>::ValueType>
        requires(!std::is_same_v<A, SyncWaitTask<T>>)
    decltype(auto) Wait(A &&awaitable)
    {
        std::atomic_flag done;
        done.clear();

        auto task = impl::MakeSyncWaitTask<A, T>(std::forward<A>(awaitable));
        task.promise().Start(done);

        done.wait(true, std::memory_order_relaxed);

        if constexpr (std::is_void_v<T>)
        {
            task.promise().Result();
            return;
        }
        else if constexpr (std::is_reference_v<T>)
        {
            return task.promise().Result();
        }
        else if constexpr (std::is_move_constructible_v<T>)
        {
            return std::move(task).promise().Result();
        }
        else
        {
            // Copy the result
            return task.promise().Result();
        }
    }

    template <typename T>
    decltype(auto) Wait(SyncWaitTask<T> &task)
    {
        std::atomic_flag done;
        done.clear();

        task.promise().Start(done);

        done.wait(false, std::memory_order_relaxed);

        if constexpr (std::is_void_v<T>)
        {
            task.promise().Result();
            return;
        }
        else if constexpr (std::is_reference_v<T>)
        {
            return task.promise().Result();
        }
        else if constexpr (std::is_move_constructible_v<T>)
        {
            return std::move(task).promise().Result();
        }
        else
        {
            // Copy the result
            return task.promise().Result();
        }
    }

} // namespace Hush::Threading