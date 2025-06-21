// RendererManager.h
#pragma once

#include "IRenderer.h"
#include "Camera.h"
#include "../Core/Runtime.h"

namespace Renderer {
	enum class RendererType {
		DirectX9,
		OpenGL21
	};

	class RendererManager {
	public:
		// call this once, before InitRenderer
		static void SelectRenderer(RendererType type);
		
		// only inits the chosen renderer
		static bool InitRenderer(Runtime::Runtime *runtime);

		static void RenderFrame();
		static void Shutdown();
		static bool SetMeshes(std::vector<std::shared_ptr<Mesh>> msh);
		static bool UpdateMesh(int indx, std::shared_ptr<Mesh> msh);

		static Camera*           cam;
		
		static IRenderer*        rendererDX9;
		static IRenderer*        rendererGL;
		
	private:
		static RendererType      s_selectedRenderer;
		static Runtime::Runtime* runtime;
	};
}
