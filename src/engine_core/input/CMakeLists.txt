# Input

hush_add_library(
        TARGET_NAME HushInput
        LIB_TYPE OBJECT
        SRCS src/InputManager.cpp
        PUBLIC_HEADER_DIRS src
)

add_library(Hush::Input ALIAS HushInput)

target_link_libraries(HushInput PUBLIC magic_enum::magic_enum HushLog)
