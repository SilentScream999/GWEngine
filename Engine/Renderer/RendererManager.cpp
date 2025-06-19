#include "RendererManager.h"
#include "RendererDX9.h"
#include "RendererGL21.h"
#include "../Core/Logger.h"
#include "Mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <memory>

namespace Renderer {
	// Static member definitions
	RendererType      RendererManager::s_selectedRenderer = RendererType::DirectX9;
	IRenderer*        RendererManager::rendererDX9        = nullptr;
	IRenderer*        RendererManager::rendererGL         = nullptr;
	Camera*           RendererManager::cam                = new Camera();
	Runtime::Runtime* RendererManager::runtime            = nullptr;

	void RendererManager::SelectRenderer(RendererType type) {
		s_selectedRenderer = type;
	}

	bool RendererManager::InitRenderer(Runtime::Runtime *runtime) {
		// Initialize camera
		cam->transform.position = glm::vec3(0.0f, 1.0f, 5.0f);
		cam->transform.rotation = glm::vec3(0.0f);

		bool success = false;

		// Initialize only the selected renderer
		if (s_selectedRenderer == RendererType::DirectX9) {
			Logger::Info("Initializing DirectX 9 renderer...");
			rendererDX9 = new RendererDX9();
			if (rendererDX9->Init(cam, runtime)) {
				Logger::Info("DirectX 9 renderer initialized.");
				success = true;
			} else {
				Logger::Error("DirectX 9 renderer failed to initialize.");
				delete rendererDX9;
				rendererDX9 = nullptr;
			}
		} else if (s_selectedRenderer == RendererType::OpenGL21) {
			Logger::Info("Initializing OpenGL 2.1 renderer...");
			rendererGL = new RendererGL21();
			if (rendererGL->Init(cam, runtime)) {
				Logger::Info("OpenGL 2.1 renderer initialized.");
				success = true;
			} else {
				Logger::Error("OpenGL 2.1 renderer failed to initialize.");
				delete rendererGL;
				rendererGL = nullptr;
			}
		}

		if (!success) {
			Logger::Error("No renderer could be initialized.");
			return false;
		}

		return true;
	}
	
	bool RendererManager::SetMeshes(std::vector<std::shared_ptr<Mesh>> meshes) {
		// Assign meshes to the active renderer
		if (rendererDX9) {
			if (rendererDX9->SetMeshes(meshes)) {
				Logger::Info("Loaded meshes for DirectX 9");
				return true;
			} else {
				Logger::Error("Could not load meshes for DirectX 9");
				return false;
			}
		}else if (rendererGL) {
			if (rendererGL->SetMeshes(meshes)) {
				Logger::Info("Loaded meshes for OpenGL 2.1");
				return true;
			} else {
				Logger::Error("Could not load meshes for OpenGL 2.1");
				return false;
			}
		}else{
			return false;
		}
	}

	void RendererManager::RenderFrame() {
		cam->updateForFrame();
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