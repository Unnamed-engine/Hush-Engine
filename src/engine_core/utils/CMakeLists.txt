# Utils

find_package(outcome CONFIG REQUIRED)

hush_add_library(
        TARGET_NAME HushUtils
        LIB_TYPE OBJECT
        SRCS src/StringUtils.cpp
             src/LibManager.cpp
             src/filesystem/PathUtils.cpp
             src/SharedLibrary.cpp
        PUBLIC_HEADER_DIRS src
)

add_library(Hush::Utils ALIAS HushUtils)

target_link_libraries(HushUtils PUBLIC HushLog outcome::hl)

if (UNIX)
    target_link_libraries(HushUtils PRIVATE dl)
endif ()
