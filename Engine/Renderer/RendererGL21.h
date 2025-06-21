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
		bool UpdateMesh(int indx, std::shared_ptr<Mesh> msh) override;
		
		ImageData CaptureFrame() override;
		void setSize(int newWidth, int newHeight);
	private:
		std::vector<std::shared_ptr<Mesh>> meshes;
		Runtime::Runtime*                  runtime;
		void CreateSkyboxTexture(const char* filename);
		
		float skybox_r = 0.0;
		float skybox_g = 0.0;
		float skybox_b = 0.0;
	};
}
