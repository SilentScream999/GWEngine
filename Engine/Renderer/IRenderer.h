#pragma once
namespace Renderer {
    class IRenderer {
    public:
        virtual bool Init() = 0;
        virtual void RenderFrame() = 0;
        virtual ~IRenderer() = default;
    };
}