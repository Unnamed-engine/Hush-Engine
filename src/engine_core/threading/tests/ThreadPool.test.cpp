/*! \file ThreadPool.test.cpp
    \author Alan Ramirez
    \date 2025-01-04
    \brief ThreadPool tests
*/

#include "ThreadPool.hpp"

#include <Logger.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <random>

using ThreadPool = Hush::Threading::ThreadPool;
template <typename T>
using Task = Hush::Threading::Task<void>;
using Job = Hush::Threading::Job;

TEST_CASE("Create ThreadPool")
{
    ThreadPool threadPool(1);

    SECTION("GetNumThreads")
    {
        REQUIRE(threadPool.GetNumThreads() == 1);
    }
}

TEST_CASE("WaitOne")
{
    ThreadPool threadPool(1);
    threadPool.Start();

    SECTION("WaitOne")
    {
        // Arrange
        std::atomic<std::uint32_t> counter = 0;
        auto threadFunction = [&counter]() -> Task<void> {
            Hush::LogInfo("Hello from thread");
            counter.fetch_add(1);
            co_return;
        };

        // Act
        auto task = threadPool.ScheduleTask(threadFunction());
        Hush::Threading::Wait(task);

        // Assert
        REQUIRE(counter.load() == 1);
    }
}

TEST_CASE("WaitUntilDone")
{
    ThreadPool threadPool(4);
    threadPool.Start();

    SECTION("WaitUntilDone")
    {
        // Arrange
        std::atomic<std::uint32_t> counter = 0;
        auto threadFunction = [&counter]() -> Task<void> {
            counter.fetch_add(1);
            co_return;
        };

        std::vector<Job> tasks;

        for (int i = 0; i < 20; ++i)
        {
            tasks.push_back(threadPool.ScheduleTask(threadFunction()));
        }

        // Act
        // Wait until all tasks are done
        for (auto &task : tasks)
        {
            Hush::Threading::Wait(task);
        }

        // Assert
        REQUIRE(counter.load() == 20);
    }
}

TEST_CASE("ScheduleFunction")
{
    ThreadPool threadPool(1);
    threadPool.Start();

    SECTION("ScheduleFunction")
    {
        // Arrange
        auto threadFunction = []() {
            (void)1;
        };

        // Act
        auto task = threadPool.ScheduleFunction(threadFunction);
        Hush::Threading::Wait(task);

        // Assert
        REQUIRE(task.GetCoroutine().done());
    }

    SECTION("ScheduleFunction lambda with captures")
    {
        // Arrange
        std::uint32_t result = 0;
        std::uint32_t a = 10;
        std::uint32_t b = 5;

        auto threadFunction = [&] {
            result = a + b;
        };
        // Act
        auto task = threadPool.ScheduleFunction(threadFunction);
        Hush::Threading::Wait(task);

        // Assert
        REQUIRE(task.GetCoroutine().done());
        REQUIRE(result == 15);
    }

    SECTION("ScheduleFunction with arguments")
    {
        // Arrange
        std::uint32_t result = 0;
        auto threadFunction = [&result](std::uint32_t a, std::uint32_t b) {
            result = a + b;
        };
        const std::uint32_t a = 10;
        const std::uint32_t b = 5;

        // Act
        auto task = threadPool.ScheduleFunction(threadFunction, a, b);
        Hush::Threading::Wait(task);

        // Assert
        REQUIRE(task.GetCoroutine().done());
        REQUIRE(result == 15);
    }
}
