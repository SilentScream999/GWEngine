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
		
		virtual void ProcessInput(GLFWwindow* window, float deltaTime) override;
		virtual void ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float deltaTime) override;
	private:
		std::vector<std::shared_ptr<Mesh>> meshes; // might as well? IDK
	};
}
