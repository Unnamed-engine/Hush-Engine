# Threading support subproject

add_library(HushThreading OBJECT
        src/ThreadPool.cpp
        src/async/SyncWait.cpp
)

target_include_directories(HushThreading PUBLIC src)

target_link_libraries(HushThreading PUBLIC HushUtils HushLog)

if (linux)
    target_link_libraries(HushThreading PUBLIC pthread)
endif()

set_all_warnings(HushThreading)

add_test_target(
        TARGET_NAME HushThreadingTest
        ENGINE_TARGET HushThreading
        SRCS tests/ThreadPool.test.cpp
        HEADER_DIRS tests
)