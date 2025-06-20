// Play.h
#pragma once

#include "Renderer/Mesh.h"
#include "Runtime.h"
#include <memory>

namespace Runtime {
	class PlayRuntime : public Runtime {
	
	public:
		bool Init() override;
		void PrepareForFrameRender() override;
		void Cleanup() override;
		
		virtual void ProcessInput(GLFWwindow* window, float deltaTime) override;
		virtual void ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float deltaTime) override;
	private:
		std::vector<std::shared_ptr<Mesh>> meshes; // might as well? IDK
	
	};
}
