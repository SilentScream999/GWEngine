#pragma once

#include "Renderer/Camera.h"
#include "vector"
#include "memory"
#include "Mesh.h"

namespace Renderer {
	class IRenderer {
	public:
		virtual bool Init(Camera *cam) = 0;
		virtual void RenderFrame() = 0;
		virtual ~IRenderer() = default;
		virtual bool SetMeshes(std::vector<std::shared_ptr<Mesh>> msh) = 0;
		
		Camera *cam;
	};
}