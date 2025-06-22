// Editor.h
#pragma once

#include "Renderer/Mesh.h"
#include "Runtime.h"
#include <memory>
#include <vector>

namespace Runtime {
	class EditorRuntime : public Runtime {
	public:
		bool Init() override;
		void PrepareForFrameRender() override;
		void Cleanup() override;
		
		virtual void ProcessInput(GLFWwindow* window, float deltaTime) override;
		virtual void ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float deltaTime) override;
		
		int AddMesh(std::string filepath, glm::vec3 pos = glm::vec3(0, 0, 0), glm::vec3 rot = glm::vec3(0, 0, 0));
		bool DeleteMesh(int meshindex);
		
		std::vector<std::shared_ptr<Mesh>> meshes; // might as well? IDK
	};
}
