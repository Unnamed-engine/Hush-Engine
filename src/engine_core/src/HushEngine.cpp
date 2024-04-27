#include "HushEngine.hpp"
#include "rendering/ImGui/VulkanImGuiForwarder.hpp"

Hush::HushEngine::~HushEngine()
{
    this->Quit();
}

void Hush::HushEngine::Run()
{
    this->m_isApplicationRunning = true;
    WindowRenderer mainRenderer(ENGINE_WINDOW_NAME.data());
    // Link our renderer with our ImGui implementation
    VulkanImGuiForwarder uiLinker;
    IRenderer *rendererImpl = mainRenderer.GetWindowRenderer();
    uiLinker.SetupImGui(rendererImpl);
    while (this->m_isApplicationRunning)
    {
        // TODO: Delta time calculations
        mainRenderer.HandleEvents(&this->m_isApplicationRunning);
        rendererImpl->Draw();
        SDL_Delay(1000 / 60);
    }
}

void Hush::HushEngine::Quit()
{
    this->m_isApplicationRunning = false;
}
