#include "Editor.h"
#include "../Renderer/RendererManager.h"

bool Runtime::EditorRuntime::Init() {
	
	
	return true;
}

void Runtime::EditorRuntime::PrepareForFrameRender() {
	// ~_~
}

void Runtime::EditorRuntime::ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float dt) {
	Renderer::RendererManager::cam->ProcessInput(xpos, ypos, KeyIsDown, dt); // later consider replacing this with a 'move omnicient?'
}

void Runtime::EditorRuntime::ProcessInput(GLFWwindow* window, float dt) {
	Renderer::RendererManager::cam->ProcessInput(window, dt); // this should later also be a 'move omnicient'
}