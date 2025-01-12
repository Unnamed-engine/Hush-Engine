/*! \file TaskTraits.hpp
    \author Alan Ramirez
    \date 2024-12-31
    \brief Task traits
*/

#pragma once

#include <coroutine>

#include "Task.hpp"

namespace Hush::Threading::Concepts
{

    ///
    /// An awaitable is a type that can be used in an await expression.
    /// It can either be something that supports await_ready, await_suspend, and await_resume, or
    /// a type that implements operator co_await.
    template <typename T>
    concept Awaiter = requires(T t, std::coroutine_handle<> h)
    {
        { t.await_ready() } -> std::same_as<bool>;
        { t.await_suspend(h) };
        { t.await_resume() };
    };

    template <typename T>
    concept LocalCoAwaitable = requires(T t)
    {
        { t.operator co_await() } -> Awaiter;
    };

    template <typename T>
    concept GlobalCoAwaitable = requires(T t)
    {
        { operator co_await (t) } -> Awaiter;
    };

    template <typename T>
    concept Awaitable = LocalCoAwaitable<T> || GlobalCoAwaitable<T> || Awaiter<T>;

    template <typename T>
    concept AwaiterVoid = Awaiter<T> && requires (T t)
    {
        {t.await_resume()} -> std::same_as<void>;
    };

    template <typename T>
    concept LocalCoAwaitableVoid = LocalCoAwaitable<T> && requires(T t)
    {
        {t. operator co_await()} -> AwaiterVoid;
    };

    template <typename T>
    concept GlobalCoAwaitableVoid = GlobalCoAwaitable<T> && requires(T t)
    {
        {operator co_await(t)} -> AwaiterVoid;
    };

    template <typename T>
    concept AwaitableVoid = LocalCoAwaitableVoid<T> || GlobalCoAwaitableVoid<T> || AwaiterVoid<T>;

    template<Awaitable A, typename = void>
    struct AwaitableTraits
    {
    };

    template <Awaitable A>
    static auto GetAwaiter(A &&awaitable)
    {
        if constexpr (LocalCoAwaitable<A>)
        {
            return std::forward<A>(awaitable).operator co_await();
        }
        else if constexpr (GlobalCoAwaitable<A>)
        {
            return operator co_await(std::forward<A>(awaitable));
        }
        else
        {
            return std::forward<A>(awaitable);
        }
    }

    template <Awaitable A>
    struct AwaitableTraits<A, std::void_t<decltype(GetAwaiter(std::declval<A>()))>>
    {
        using AwaiterType = decltype(GetAwaiter(std::declval<A>()));
        using ResultType = decltype(std::declval<AwaiterType>().await_resume());
    };
} // namespace Hush