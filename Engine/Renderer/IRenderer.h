#pragma once

#include "Renderer/Camera.h"
#include "vector"
#include "memory"
#include "Mesh.h"
#include "../Core/Runtime.h"

namespace Renderer {
	struct ImageData {
		std::vector<unsigned char> pixels;
		int width, height;
	};
	
	class IRenderer {
	public:
		virtual bool Init(Camera *cam, Runtime::Runtime *runtime) = 0;
		virtual void RenderFrame() = 0;
		virtual ~IRenderer() = default;
		virtual bool SetMeshes(std::vector<std::shared_ptr<Mesh>> msh) = 0;
		virtual bool UpdateMesh(int indx, std::shared_ptr<Mesh> msh) = 0;
		virtual bool DeleteMesh(int indx) = 0;
		virtual int AddMesh(std::shared_ptr<Mesh> mesh) = 0;
	
		virtual ImageData CaptureFrame() = 0;
		virtual void setSize(int newWidth, int newHeight) = 0;
		
		Camera *cam;
	};
}