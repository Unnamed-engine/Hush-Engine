# Threading support subproject

hush_add_library(
        TARGET_NAME HushThreading
        LIB_TYPE OBJECT
        SRCS src/ThreadPool.cpp src/async/SyncWait.cpp
        PUBLIC_HEADER_DIRS src
)
add_library(Hush::Threading ALIAS HushThreading)

target_link_libraries(HushThreading PUBLIC Hush::Log Hush::Utils)

if (linux)
    target_link_libraries(HushThreading PUBLIC pthread)
endif()

add_test_target(
        TARGET_NAME HushThreadingTest
        ENGINE_TARGET HushThreading
        SRCS tests/ThreadPool.test.cpp
        HEADER_DIRS tests
)