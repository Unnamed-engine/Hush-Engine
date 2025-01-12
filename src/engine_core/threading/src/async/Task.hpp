/*! \file Task.hpp
    \author Alan Ramirez
    \date 2024-12-25
    \brief Job implementation
*/

#pragma once

#include <Logger.hpp>
#include <assert.h>
#include <coroutine>
#include <variant>

namespace Hush::Threading
{
    template <typename T>
    struct Task;

    namespace impl
    {
        struct PromiseBase
        {
            std::coroutine_handle<> continuation;

            struct FinalAwaiter
            {
                bool await_ready() const noexcept
                {
                    return false;
                }

                template <typename PromiseType>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> coroutine) noexcept
                {
                    std::coroutine_handle<> coroutineContinuation = coroutine.promise().continuation;
                    if (!coroutineContinuation)
                    {
                        return std::noop_coroutine();
                    }

                    return coroutineContinuation;
                }

                void await_resume() noexcept
                {
                }
            };

            PromiseBase() noexcept = default;

            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            FinalAwaiter final_suspend() noexcept
            {
                return {};
            }

            void SetContinuation(std::coroutine_handle<> coroContinuation) noexcept
            {
                this->continuation = coroContinuation;
            }
        };

        template <typename T>
        struct TaskPromise : public PromiseBase
        {
            TaskPromise() noexcept
            {
            }

            Task<T> get_return_object() noexcept;

            ~TaskPromise()
            {
                switch (m_state)
                {
                case EState::Value:
                    m_value.~T();
                    break;
                case EState::Exception:
                    m_exception.~exception_ptr();
                    break;
                }
            }

            T &Result() &&
            {
                if (m_state == EState::Exception)
                {
                    std::rethrow_exception(m_exception);
                }

                return std::move(m_value);
            }

            T &Result() &
            {
                if (m_state == EState::Exception)
                {
                    std::rethrow_exception(m_exception);
                }

                return m_value;
            }

            void return_value(T &&value) noexcept
            {
                ::new (static_cast<void *>(std::addressof(m_value))) T(std::move(value));
                m_state = Value;
            }

            void unhandled_exception() noexcept
            {
                ::new (static_cast<void *>(std::addressof(m_exception))) std::exception_ptr(std::current_exception());
                m_state = Exception;
            }

        private:
            enum EState
            {
                Value,
                Exception
            };
            union {
                T m_value;
                std::exception_ptr m_exception;
            };
            EState m_state = Value;
        };

        template <>
        struct TaskPromise<void> : public PromiseBase
        {
            TaskPromise() noexcept
            {
            }

            Task<void> get_return_object() noexcept;

            void return_void() noexcept
            {
            }

            void unhandled_exception() noexcept
            {
                m_exception = std::current_exception();
            }

            void Result() noexcept
            {
                if (m_exception)
                {
                    std::rethrow_exception(m_exception);
                }
            }

        private:
            std::exception_ptr m_exception;
        };

        template <typename T>
        struct TaskPromise<T &> : public PromiseBase
        {
            Task<T &> get_return_object() noexcept;

            T &Result()
            {
                if (m_exception != nullptr)
                {
                    std::rethrow_exception(m_exception);
                }

                return *m_value;
            }

            void unhandled_exception() noexcept
            {
                m_exception = std::current_exception();
            }

        private:
            T *m_value = nullptr;
            std::exception_ptr m_exception = nullptr;
        };

    } // namespace impl

    template <typename T>
     struct [[nodiscard]] Task final
    {
        using promise_type = impl::TaskPromise<T>;
        using value_type = T;

    private:
        struct InitialAwaiterBase
        {

            std::coroutine_handle<promise_type> coroutine;

            explicit InitialAwaiterBase(std::coroutine_handle<promise_type> coroutine) noexcept
                : coroutine(coroutine)
            {
            }

            bool await_ready() const noexcept
            {
                return !coroutine || coroutine.done();
            }

            auto await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
            {
                coroutine.promise().SetContinuation(awaitingCoroutine);
                return coroutine;
            }
        };

    public:
        Task() noexcept
            : m_coroutine(nullptr)
        {
        }

        explicit Task(std::coroutine_handle<promise_type> coroutine) noexcept
            : m_coroutine(coroutine)
        {
        }

        Task(Task &&other) noexcept
            : m_coroutine(other.m_coroutine)
        {
            other.m_coroutine = nullptr;
        }

        Task(const Task &) = delete;

        Task &operator=(Task &&other) noexcept
        {
            if (this != std::addressof(other))
            {
                m_coroutine = other.m_coroutine;
                other.m_coroutine = nullptr;
            }

            return *this;
        }

        Task &operator=(const Task &) = delete;

        ~Task()
        {
            if (m_coroutine)
            {
                m_coroutine.destroy();
            }
        }

        bool Ready() const noexcept
        {
            return !m_coroutine || m_coroutine.done();
        }

        auto operator co_await() const & noexcept
        {
            struct Awaiter : public InitialAwaiterBase
            {
                explicit Awaiter(std::coroutine_handle<promise_type> coroutine) noexcept
                    : InitialAwaiterBase(coroutine)
                {
                }

                decltype(auto) await_resume()
                {
                    assert(this->coroutine != nullptr);

                    return this->coroutine.promise().Result();
                }
            };

            return Awaiter{m_coroutine};
        }

        template <typename U = T>
            requires !std::is_void_v<U>
                     auto operator co_await() const &&
            noexcept
        {
            struct Awaiter : InitialAwaiterBase
            {
                explicit Awaiter(std::coroutine_handle<promise_type> coroutine) noexcept
                    : InitialAwaiterBase(coroutine)
                {
                }
                decltype(auto) await_resume()
                {
                    assert(this->coroutine != nullptr);

                    return std::move(this->coroutine.promise().Result());
                }
            };

            return Awaiter{m_coroutine};
        }

        std::coroutine_handle<promise_type> GetCoroutine() const noexcept
        {
            return m_coroutine;
        }

    private:
        std::coroutine_handle<promise_type> m_coroutine = nullptr;
    };

    namespace impl
    {
        template <typename T>
        Task<T> TaskPromise<T>::get_return_object() noexcept
        {
            return Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
        }

        template <typename T>
        Task<T &> TaskPromise<T &>::get_return_object() noexcept
        {
            return Task<T &>{std::coroutine_handle<TaskPromise<T &>>::from_promise(*this)};
        }

        Task<void> inline TaskPromise<void>::get_return_object() noexcept
        {
            return Task<void>{std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
        }

    } // namespace impl
} // namespace Hush