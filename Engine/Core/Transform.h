#pragma once
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

struct Transform {
	glm::vec3 position    = glm::vec3(0.0f);
	glm::vec3 rotation    = glm::vec3(0.0f);  // pitch (x), yaw (y), roll (z)
	glm::vec3 scale       = glm::vec3(1.0f);

	glm::mat4 RotationMatrix() const {
		return glm::yawPitchRoll(rotation.y, rotation.x, rotation.z);
	}

	glm::vec3 GetForward() const {
		return glm::normalize(glm::vec3(RotationMatrix() * glm::vec4(0, 0, -1, 0)));
	}

	glm::vec3 GetRight() const {
		return glm::normalize(glm::cross(GetForward(), glm::vec3(0,1,0)));
	}

	glm::vec3 GetUp() const {
		return glm::vec3(0,1,0);
	}

	// Move in object-space by localOffset (e.g. (0,0,-1) is forward)
	void TranslateBy(const glm::vec3& localOffset) {
		glm::vec3 worldOffset = glm::vec3(RotationMatrix() * glm::vec4(localOffset, 0.0f));
		position += worldOffset;
	}
	
	void RotateBy(const glm::vec3& localOffset) {
		rotation += localOffset;
	}

	// Instantly rotate so +Z (forward) faces target
	void LookAt(const glm::vec3& targetPos) {
		glm::vec3 dir = glm::normalize(targetPos - position);
		rotation.y = std::atan2(dir.x, -dir.z);
		rotation.x = std::asin( std::clamp(dir.y, -1.0f, 1.0f) );
		// leave roll unchanged
	}

	glm::mat4 ModelMatrix() const {
		glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
		m *= RotationMatrix();
		return glm::scale(m, scale);
	}
};
