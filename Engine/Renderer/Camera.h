#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include "../Core/Transform.h"

class Camera {
public:
	Camera();

	// Public state through transform
	Transform transform;
	glm::vec3 lookAtPosition;  // computed each frame (transform.position + forward)

	// Tuning parameters
	float movementSpeed    = 5.0f;   // units per second
	float mouseSensitivity = 0.1f;   // degrees per pixel

	// For GLFW mouse handling
	bool   firstMouse = true;
	double lastX      = 0.0;
	double lastY      = 0.0;

	// Call once per frame after input to update lookAtPosition
	void updateForFrame();

	// For GLFW/GL input path: call every frame with the GLFWwindow and deltaTime
	void ProcessInput(GLFWwindow* window, float deltaTime);
	void ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float deltaTime);

private:
	static glm::vec3 CalcLookAt(const glm::vec3& position, const glm::vec3& rotationEulerRadians);
};
