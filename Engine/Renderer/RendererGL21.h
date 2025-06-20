// RendererGL21.h
#pragma once

#include "IRenderer.h"
#include "Mesh.h"
#include "Renderer/Camera.h"
#include <memory>           // for std::shared_ptr

namespace Renderer {
	class RendererGL21 : public IRenderer {
	public:
		bool Init(Camera *cam, Runtime::Runtime *runtime) override;
		void RenderFrame() override;
		bool SetMeshes(std::vector<std::shared_ptr<Mesh>> msh) override;
		
		ImageData CaptureFrame() override;
		void setSize(int newWidth, int newHeight);
	private:
		std::vector<std::shared_ptr<Mesh>> meshes;
		Runtime::Runtime*                  runtime;
	};
}
