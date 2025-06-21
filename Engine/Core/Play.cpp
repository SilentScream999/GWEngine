#include "Play.h"
#include "../Renderer/RendererManager.h"
#include "Logger.h"

bool Runtime::PlayRuntime::Init() {
	// Load meshes
	auto mesh1 = std::make_shared<Mesh>();
	if (!mesh1->LoadFromOBJ("assets/models/test2.obj")) {
		Logger::Error("Failed to load OBJ test2.obj");
		return false;
	}
	mesh1->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
	meshes.push_back(mesh1);

	auto mesh2 = std::make_shared<Mesh>();
	if (!mesh2->LoadFromOBJ("assets/models/test.obj")) {
		Logger::Error("Failed to load OBJ test.obj");
		return false;
	}
	mesh2->transform.position = glm::vec3(1.0f, 1.0f, 1.0f);
	meshes.push_back(mesh2);
	
	Renderer::RendererManager::SetMeshes(meshes);
	
	return true;
}

void Runtime::PlayRuntime::PrepareForFrameRender() {
	// right here we are testing just messing with the positioning and rotation of the meshes.
	
	meshes[0]->transform.TranslateBy(glm::vec3(0, 0.01, 0));
	meshes[1]->transform.RotateBy(glm::vec3(0, 0.1, 0.01));
	Renderer::RendererManager::UpdateMesh(0, meshes[0]);
}

void Runtime::PlayRuntime::Cleanup() {
	
}

void Runtime::PlayRuntime::ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float dt) {
	Renderer::RendererManager::cam->ProcessInput(xpos, ypos, KeyIsDown, dt); // later consider replacing this with a 'move omnicient?'
}

void Runtime::PlayRuntime::ProcessInput(GLFWwindow* window, float dt) {
	Renderer::RendererManager::cam->ProcessInput(window, dt); // this should later also be a 'move omnicient'
}