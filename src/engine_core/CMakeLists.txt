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

add_library(HushEngine STATIC
        src/main.cpp
        src/HushEngine.cpp
)

target_include_directories(HushEngine PRIVATE src)

add_executable(HushRuntime src/dummy.cpp)

target_link_libraries(HushRuntime PRIVATE HushEngine)

set(ENGINE_LIBS
        coreclr
        HushAppLoader
        HushInput
        HushLog
        HushRendering
        HushUtils
        HushCSharp
        HushFilesystem
        HushThreading
)

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

target_link_libraries(HushEngine PUBLIC ${ENGINE_LIBS})

set_all_warnings(HushEngine)

#Copy the .NET runtime configuration
set(RUNTIME_CONFIG_SOURCE_DIR ../editor/assembly-test/bin)
#Check if we're in a single or multiple config generator
if (CMAKE_CONFIGURATION_TYPES)
    set(BUILD_TYPE $<CONFIG>)
else ()
    set(BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif ()
