// RendererManager.h
#pragma once
#include <vector>
#include <memory>
#include "IRenderer.h"
#include "Camera.h"

namespace Renderer {
    enum class RendererType {
        DirectX9,
        OpenGL21
    };

    class RendererManager {
    public:
        // call this once, before InitRenderer()
        static void SelectRenderer(RendererType type);

        // only inits the chosen renderer
        static bool InitRenderer();

        static void RenderFrame();
        static void Shutdown();

    private:
        static RendererType s_selectedRenderer;
        static IRenderer* rendererDX9;
        static IRenderer* rendererGL;
        static Camera*     cam;
    };
}
