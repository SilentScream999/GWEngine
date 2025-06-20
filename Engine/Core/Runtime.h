#pragma once

/*
Here's the game plan, we're going to make it such that the main thing - whatever that is (probably main.cpp?) - calls the apropriate runtime (editor/playtime)
*/

#include <GLFW/glfw3.h>
#include <functional>
namespace Runtime {
	class Runtime {
	public:
		virtual bool Init() = 0;
		virtual void PrepareForFrameRender() = 0;
		virtual void Cleanup() = 0;
		
		virtual void ProcessInput(GLFWwindow* window, float deltaTime) = 0; // my fancy pantsy overloaded functions... my my...
		virtual void ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float deltaTime) = 0;
	};
}