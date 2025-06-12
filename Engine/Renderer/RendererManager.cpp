#include "RendererManager.h"
#include "RendererGL21.h"
#include "RendererDX9.h"
#include "../Core/Logger.h"

namespace Renderer {
    static IRenderer* rendererDX9 = nullptr;
    static IRenderer* rendererGL  = nullptr;

    bool RendererManager::InitRenderer() {
        bool anySuccess = false;

        Logger::Info("Initializing DirectX 9 renderer...");
        rendererDX9 = new RendererDX9();
        if (rendererDX9->Init()) {
            Logger::Info("DirectX 9 renderer initialized.");
            anySuccess = true;
        } else {
            Logger::Error("DirectX 9 renderer failed to initialize.");
            delete rendererDX9;
            rendererDX9 = nullptr;
        }

        Logger::Info("Initializing OpenGL 2.1 renderer...");
        rendererGL = new RendererGL21();
        if (rendererGL->Init()) {
            Logger::Info("OpenGL 2.1 renderer initialized.");
            anySuccess = true;
        } else {
            Logger::Error("OpenGL 2.1 renderer failed to initialize.");
            delete rendererGL;
            rendererGL = nullptr;
        }

        if (!anySuccess)
            Logger::Error("No renderer could be initialized.");

        return anySuccess;
    }

    void RendererManager::RenderFrame() {
        if (rendererDX9) rendererDX9->RenderFrame();
        if (rendererGL)  rendererGL->RenderFrame();
    }

    void RendererManager::Shutdown() {
        delete rendererDX9;
        delete rendererGL;
        rendererDX9 = nullptr;
        rendererGL  = nullptr;
    }
}
