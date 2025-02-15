# VFS implementation

hush_add_library(
        TARGET_NAME HushFilesystem
        LIB_TYPE OBJECT
        SRCS src/filesystem/CFileSystem/CFileSystem.cpp
             src/filesystem/CFileSystem/CFile.cpp
             src/VirtualFilesystem.cpp
             src/IFile.cpp
        PUBLIC_HEADER_DIRS src
)

add_library(Hush::Filesystem ALIAS HushFilesystem)

target_link_libraries(HushFilesystem PUBLIC HushUtils HushLog)

add_test_target(
        TARGET_NAME HushFilesystemTest
        ENGINE_TARGET HushFilesystem
        SRCS tests/filesystem/CFileSystem/CFile.test.cpp tests/filesystem/CFileSystem/CFileSystem.test.cpp
        HEADER_DIRS tests
)
