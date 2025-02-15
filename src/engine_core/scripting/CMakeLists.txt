# Scripting

hush_add_library(
        TARGET_NAME HushCSharp
        LIB_TYPE OBJECT
        SRCS src/DotnetHost.cpp src/ScriptingManager.cpp
        PUBLIC_HEADER_DIRS src
)

add_library(Hush::CSharp ALIAS HushCSharp)

target_link_libraries(HushCSharp PUBLIC HushUtils HushLog coreclr)
