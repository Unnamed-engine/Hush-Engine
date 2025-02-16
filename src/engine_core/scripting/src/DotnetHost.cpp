#include "DotnetHost.hpp"

#include "Logger.hpp"
#include "LibManager.hpp"
#include "StringUtils.hpp"
#include "filesystem/PathUtils.hpp"
#include "Platform.hpp"

constexpr std::string_view DOTNET_CMD("hostfxr_initialize_for_dotnet_command_line");
constexpr std::string_view DOTNET_RUNTIME_INIT_CONFIG("hostfxr_initialize_for_runtime_config");
constexpr std::string_view DOTNET_RUNTIME_DELEGATE("hostfxr_get_runtime_delegate");
constexpr std::string_view DOTNET_RUN_FUNCTION("hostfxr_run_app");
constexpr std::string_view DOTNET_CLOSE_FUNCTION("hostfxr_close");
constexpr std::string_view DOTNET_ERROR_WRITER("hostfxr_set_error_writer");

constexpr std::string_view RUNTIME_CONFIG_JSON("assembly-test.runtimeconfig.json");
constexpr std::string_view HOST_FXR_FIRST_PATH("host/fxr/");

Hush::DotnetHost::DotnetHost(const char *dotnetPath)
{
    // We want to find a substring in the path that will lead us to a supported version of .NET hostfxr
    // for now we will do the first subdir that contains this given substr
    const char *targetPathSubstring = "8.";
    std::filesystem::path targetPath = dotnetPath;
    //
    targetPath /= HOST_FXR_FIRST_PATH;
    bool appended = PathUtils::FindAndAppendSubDirectory(targetPath, targetPathSubstring);
    if (!appended)
    {
        Hush::LogError("No valid host was found for a .NET 8 version, make sure you have .NET 8 installed");
        return;
    }
#if HUSH_PLATFORM_WIN
    std::string strLibPath = targetPath.string();
    const char *libPath = strLibPath.c_str();
#else
    const char *libPath = targetPath.c_str();
#endif
    void *sharedLibrary = LibManager::LibraryOpen(libPath);
    if (sharedLibrary == nullptr)
    {
        Hush::LogFormat(Hush::ELogLevel::Error, "Failed to load {}", libPath);
        return;
    }
    // TODO: See which of these can stop being cached and just pass them as params to the initdotnetcore
    this->m_cmdLineFuncPtr =
        LoadSymbol<hostfxr_initialize_for_dotnet_command_line_fn>(sharedLibrary, DOTNET_CMD.data());
    this->m_initFuncPtr =
        LoadSymbol<hostfxr_initialize_for_runtime_config_fn>(sharedLibrary, DOTNET_RUNTIME_INIT_CONFIG.data());
    this->m_getDelegateFuncPtr =
        LoadSymbol<hostfxr_get_runtime_delegate_fn>(sharedLibrary, DOTNET_RUNTIME_DELEGATE.data());
    this->m_runAppFuncPtr = LoadSymbol<hostfxr_run_app_fn>(sharedLibrary, DOTNET_RUN_FUNCTION.data());
    this->m_closeFuncPtr = LoadSymbol<hostfxr_close_fn>(sharedLibrary, DOTNET_CLOSE_FUNCTION.data());
    this->m_errorWriterFuncPtr = LoadSymbol<hostfxr_set_error_writer_fn>(sharedLibrary, DOTNET_ERROR_WRITER.data());
    // Add logging for any errors in C#
#if HUSH_PLATFORM_WIN
    this->m_errorWriterFuncPtr([](const char_t *message) {
        std::wstring wStrMessage = message;
        std::string strMessage = StringUtils::FromWString(wStrMessage);
        const char *cMessage = strMessage.c_str();
        Hush::LogFormat(Hush::ELogLevel::Error, "Received an error from C# runtime {}", cMessage);
    });
#else
    this->m_errorWriterFuncPtr([](const char *message) {
        Hush::LogFormat(Hush::ELogLevel::Error, "Received an error from C# runtime {}", message);
    });
#endif
    this->InitDotnetCore();
}

Hush::DotnetHost::~DotnetHost()
{
    this->m_closeFuncPtr(this->m_hostFxrHandle);
}

get_function_pointer_fn Hush::DotnetHost::GetFunctionGetterFuncPtr()
{
    return this->m_functionGetterFuncPtr;
}

void Hush::DotnetHost::InitDotnetCore()
{
    std::filesystem::path runtimeConfigPath = LibManager::GetCurrentExecutablePath();
    runtimeConfigPath /= RUNTIME_CONFIG_JSON.data();
#if _WIN32
    std::wstring configStr = runtimeConfigPath.wstring();
    const char_t *runtimeConfig = configStr.data();
#else
    const char *runtimeConfig = runtimeConfigPath.c_str();
#endif

    // Load and initialize .NET Core
    int rc = this->m_initFuncPtr(runtimeConfig, nullptr, &this->m_hostFxrHandle);
    if (rc != 0)
    {
        Hush::LogFormat(Hush::ELogLevel::Error, "Failed to initialize .NET core with error code {}", rc);
        return;
    }
    Hush::LogTrace("Init .NET core succeeded!");
    load_assembly_fn assemblyLoader = this->GetLoadAssembly(this->m_hostFxrHandle);

    if (assemblyLoader == nullptr)
    {
        Hush::LogError("Could not get load assembly ptr");
        return;
    }

    this->m_functionGetterFuncPtr = this->GetFunctionPtr(this->m_hostFxrHandle);

    if (this->m_functionGetterFuncPtr == nullptr)
    {
        Hush::LogError("Could not get function ptr");
        return;
    }
    // Actually load the assembly
    bool isAssemblyLoaded = LoadAssemblyFromPath(assemblyLoader);

    if (!isAssemblyLoaded)
    {
        Hush::LogError("Failed to load the assembly");
        return;
    }
}

load_assembly_fn Hush::DotnetHost::GetLoadAssembly(void *hostFxrHandle)
{
    // Get the load_assembly_and_get_function_pointer function pointer
    load_assembly_fn loadAssembly = nullptr;
    this->m_getDelegateFuncPtr(hostFxrHandle, hdt_load_assembly, reinterpret_cast<void **>(&loadAssembly));
    return loadAssembly;
}

get_function_pointer_fn Hush::DotnetHost::GetFunctionPtr(void *hostFxrHandle)
{
    get_function_pointer_fn getFunctionPointer = nullptr;
    m_getDelegateFuncPtr(hostFxrHandle, hdt_get_function_pointer, reinterpret_cast<void **>(&getFunctionPointer));
    return getFunctionPointer;
}

bool Hush::DotnetHost::LoadAssemblyFromPath(load_assembly_fn assemblyLoader)
{
    std::filesystem::path assemblyPath = LibManager::GetCurrentExecutablePath() / "assembly-test.dll";
    int rc = assemblyLoader(assemblyPath.c_str(), nullptr, nullptr);
    return rc == 0;
}

template <class T> T Hush::DotnetHost::LoadSymbol(void *sharedLibrary, const char *name)
{
    void *libraryPtr = LibManager::DynamicLoadSymbol(sharedLibrary, name);
    return reinterpret_cast<T>(libraryPtr);
}
