include(cmake/deps.cmake)
include(cmake/utils.cmake)

add_subdirectory(app_loader)
add_subdirectory(input)
add_subdirectory(log)
add_subdirectory(rendering)
add_subdirectory(utils)
add_subdirectory(scripting)
add_subdirectory(resources)
add_subdirectory(filesystem)
add_subdirectory(threading)
add_subdirectory(core)

hush_add_library(
        TARGET_NAME HushEngine
        LIB_TYPE STATIC
        SRCS src/main.cpp src/HushEngine.cpp
        PUBLIC_HEADER_DIRS src
)

add_library(Hush::Engine ALIAS HushEngine)

target_link_libraries(HushEngine PUBLIC coreclr
        Hush::AppLoader
        Hush::Input
        Hush::Log
        Hush::Rendering
        Hush::Utils
        Hush::CSharp
        Hush::Resources
        Hush::Filesystem
        Hush::Threading
        Hush::Core
        HushAppLoader
)

hush_add_executable(
        TARGET_NAME HushRuntime
        SRCS src/dummy.cpp
)

target_link_libraries(HushRuntime PRIVATE Hush::Engine)

if (WIN32)
    set_target_properties(HushEngine PROPERTIES
            VS_GLOBAL_RunCodeAnalysis false

            # Use visual studio core guidelines
            VS_GLOBAL_EnableMicrosoftCodeAnalysis false
            #VS_GLOBAL_CodeAnalysisRuleSet ${CMAKE_CURRENT_SOURCE_DIR}/foo.ruleset
            #VS_GLOBAL_CodeAnalysisRuleSet ${CMAKE_CURRENT_SOURCE_DIR}/foo.ruleset

            # Use clangtidy
            VS_GLOBAL_EnableClangTidyCodeAnalysis true
            VS_GLOBAL_ClangTidyChecks "-* \"\"--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy --header-filter=.*"
    )
endif ()

#Copy the .NET runtime configuration
set(RUNTIME_CONFIG_SOURCE_DIR ../editor/assembly-test/bin)
#Check if we're in a single or multiple config generator
if (CMAKE_CONFIGURATION_TYPES)
    set(BUILD_TYPE $<CONFIG>)
else ()
    set(BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif ()
