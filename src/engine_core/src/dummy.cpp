#include "ApplicationLoader.hpp"
#include "../internal/AppSupport.hpp"

extern "C" bool BundledAppExists_Internal_() HUSH_WEAK
{
    return false;
}

extern "C" Hush::IApplication *BundledApp_Internal_(Hush::HushEngine *engine)
{
    return nullptr;
}