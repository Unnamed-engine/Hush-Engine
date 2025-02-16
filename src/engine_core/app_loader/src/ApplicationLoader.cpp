/*! \file ApplicationLoader.cpp
\author Alan Ramirez
    \date 2024-09-21
    \brief Helper to load an application in hush
*/

#include "ApplicationLoader.hpp"
#include "../../src/HushEngine.hpp"
#include "AppSupport.hpp"
#include "Platform.hpp"

#include <Logger.hpp>

extern "C" bool BundledAppExists_Internal_() HUSH_WEAK;

extern "C" Hush::IApplication *BundledApp_Internal_(Hush::HushEngine *engine) HUSH_WEAK;

std::unique_ptr<Hush::IApplication> Hush::LoadApplication(HushEngine *engine)
{
    // First, check if platform supports shared library app. If not, just attempt to load the bundled app.
#if !HUSH_SUPPORTS_SHARED_APP
    return BundledApp__Internal();
#else
    // Ok, we support apps as shared libraries, we then must check if a bundled application exists.
    if (BundledAppExists_Internal_())
    {
        // It exists, just return it.
        return std::unique_ptr<IApplication>(BundledApp_Internal_(engine));
    }

    // We can't find it, attempt to load it through a shared library.
    // TODO: define file metadata????

    return nullptr;
#endif
}