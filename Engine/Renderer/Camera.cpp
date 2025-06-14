#include "Camera.h"
#define GLM_ENABLE_EXPERIMENTAL // otherwise it bitches

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

Camera::Camera(){}

void Camera::updateForFrame() {
	// we need to calculate the look-at position based on the position and the rotation. both of which are glm vec3s
	lookAtPosition = CalcLookAt(position, rotation);
}

glm::vec3 Camera::CalcLookAt(const glm::vec3& position, const glm::vec3& rotationEulerRadians) { // ala research team (also said team says use rads with this)
	// Create a rotation matrix from Euler angles (pitch = x, yaw = y, roll = z)
	glm::mat4 rotMatrix = glm::yawPitchRoll(rotationEulerRadians.y, rotationEulerRadians.x, rotationEulerRadians.z);
	
	// The forward vector in local space
	glm::vec3 forward = glm::vec3(rotMatrix * glm::vec4(0, 0, -1, 0)); // -Z is forward in OpenGL
	
	// Add forward direction to position to get look-at point
	return position + forward;
}