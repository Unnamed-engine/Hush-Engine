# Log

hush_add_library(
        TARGET_NAME HushLog
        LIB_TYPE OBJECT
        SRCS src/Logger.cpp
        PUBLIC_HEADER_DIRS src
)

add_library(Hush::Log ALIAS HushLog)

target_link_libraries(HushLog PUBLIC fmt::fmt spdlog::spdlog magic_enum::magic_enum)
