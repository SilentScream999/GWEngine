#include "RendererManager.h"
#include "RendererGL21.h"
#include "RendererDX9.h"
#include "../Core/Logger.h"

namespace Renderer {
	static IRenderer* rendererDX9 = nullptr;
	static IRenderer* rendererGL  = nullptr;
	static Camera* cam = new Camera();
	static float PI = 3.14159f;
	
	bool RendererManager::InitRenderer() {
		cam->position = *new glm::vec3(0, 1, 5); // first is horizontal -, second is vertical |, third is forward/back .
		cam->rotation = *new glm::vec3(0, 0, 0); // this is in rads - | . (I think) or . | -
		
		bool anySuccess = false;
		
		// initialize the rendering engines
		
		Logger::Info("Initializing DirectX 9 renderer...");
		rendererDX9 = new RendererDX9();
		if (rendererDX9->Init(cam)) {
			Logger::Info("DirectX 9 renderer initialized.");
			anySuccess = true;
		} else {
			Logger::Error("DirectX 9 renderer failed to initialize.");
			delete rendererDX9;
			rendererDX9 = nullptr;
		}
		
		Logger::Info("Initializing OpenGL 2.1 renderer...");
		rendererGL = new RendererGL21();
		if (rendererGL->Init(cam)) {
			Logger::Info("OpenGL 2.1 renderer initialized.");
			anySuccess = true;
		} else {
			Logger::Error("OpenGL 2.1 renderer failed to initialize.");
			delete rendererGL;
			rendererGL = nullptr;
		}
		
		// load the mesh(es)
		
		std::vector<std::shared_ptr<Mesh>> meshes;
		auto mesh = std::make_shared<Mesh>();
		if (!mesh->LoadFromOBJ("assets/models/test2.obj")) {
			Logger::Error("Failed to load OBJ test2.");
			return false;
		}
		mesh->position = *new glm::vec3(0, 0, 0);
		meshes.push_back(mesh);
		
		auto mesh2 = std::make_shared<Mesh>();
		if (!mesh2->LoadFromOBJ("assets/models/test.obj")) {
			Logger::Error("Failed to load OBJ test.");
			return false;
		}
		mesh2->position = *new glm::vec3(1, 1, 1);
		meshes.push_back(mesh2);
		
		// setting the mesh(es) to each renderer
		
		if (rendererDX9 && rendererDX9->SetMeshes(meshes)) {
			Logger::Info("Loaded meshes for DX9");
		}else if (rendererDX9){ // we only send the error if the dx9 renderer is intialized at all
			Logger::Error("Could not load meshes for DX9");
		}
		
		if (rendererGL && rendererGL->SetMeshes(meshes)) {
			Logger::Info("Loaded meshes for OpenGL 2.1");
		}else if (rendererGL){ // we only send the error if the opengl renderer is intialized at all
			Logger::Error("Could not load meshes for OpenGL 2.1");
		}
		
		// did the things here
		
		if (!anySuccess) // the devil's if statement... kai? practicing satanism? hmm...
			Logger::Error("No renderer could be initialized.");

		return anySuccess;
	}

	void RendererManager::RenderFrame() {
		// this here is a test of the camera rotations
		cam->rotation.y += PI*(1.0/180.0);
		if (cam->rotation.y > PI*(30.0/180.0)) {
			cam->rotation.y = -PI*(30.0/180.0);
		}
		
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