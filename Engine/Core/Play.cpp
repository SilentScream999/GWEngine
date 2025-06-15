#include "Play.h"
#include "../Renderer/RendererManager.h"
#include "Logger.h"

bool Runtime::PlayRuntime::Init() {
	// Load meshes
	std::vector<std::shared_ptr<Mesh>> meshes;

	auto mesh1 = std::make_shared<Mesh>();
	if (!mesh1->LoadFromOBJ("assets/models/test2.obj")) {
		Logger::Error("Failed to load OBJ test2.obj");
		return false;
	}
	mesh1->position = glm::vec3(0.0f, 0.0f, 0.0f);
	meshes.push_back(mesh1);

	auto mesh2 = std::make_shared<Mesh>();
	if (!mesh2->LoadFromOBJ("assets/models/test.obj")) {
		Logger::Error("Failed to load OBJ test.obj");
		return false;
	}
	mesh2->position = glm::vec3(1.0f, 1.0f, 1.0f);
	meshes.push_back(mesh2);
	
	Renderer::RendererManager::SetMeshes(meshes);
	
	return true;
}

void Runtime::PlayRuntime::PrepareForFrameRender() {
	// nothing to do here I spose? for now
}

void Runtime::PlayRuntime::ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float dt) {
	Renderer::RendererManager::cam->ProcessInput(xpos, ypos, KeyIsDown, dt); // later consider replacing this with a 'move omnicient?'
}

void Runtime::PlayRuntime::ProcessInput(GLFWwindow* window, float dt) {
	Renderer::RendererManager::cam->ProcessInput(window, dt); // this should later also be a 'move omnicient'
}