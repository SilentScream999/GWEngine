#pragma once

namespace Renderer {
    class RendererManager {
    public:
        static bool InitRenderer();
        static void RenderFrame();
        static void Shutdown();  // <-- Add this
    };
}
