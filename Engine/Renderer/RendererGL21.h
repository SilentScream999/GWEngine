// RendererGL21.h
#pragma once
#include "IRenderer.h"
#include "Mesh.h"
#include <memory>           // for std::shared_ptr

namespace Renderer {
    class RendererGL21 : public IRenderer {
    public:
        bool Init() override;
        void RenderFrame() override;

    private:
        std::shared_ptr<Mesh> mesh;
    };
}
