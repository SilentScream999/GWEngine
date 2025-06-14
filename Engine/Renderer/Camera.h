#pragma once

#include <glm/glm.hpp>

class Camera {
public:
	// two-arg constructor so emplace_back in Model.cpp works
	Camera();
	
	glm::vec3 position;
	glm::vec3 rotation; // we can add more here if and when they become a problem. (fpv?)
	glm::vec3 lookAtPosition; // this is for the rendering, we calculate this in updateForFrame
	
	glm::vec3 CalcLookAt(const glm::vec3& position, const glm::vec3& rotationEulerRadians);
	void updateForFrame(); // call this before every frame
};