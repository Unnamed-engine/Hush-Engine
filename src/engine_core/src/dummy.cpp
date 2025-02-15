#include "../internal/AppSupport.hpp"
#include "ApplicationLoader.hpp"

extern "C" bool BundledAppExists_Internal_() HUSH_WEAK
{
    return false;
}

extern "C" Hush::IApplication *BundledApp_Internal_(Hush::HushEngine *engine){
    (void)engine;
    return nullptr;
}