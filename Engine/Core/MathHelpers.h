#pragma once
#include "Renderer/Mesh.h"
#include <glm/glm.hpp>
#include <iostream>
#include <memory>
#include <optional>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Ray.h"

namespace MathHelpers {
	inline glm::mat4 EulerToMatrix(const glm::vec3& euler) {
		return glm::yawPitchRoll(euler.y, euler.x, euler.z);
	}

	inline glm::vec3 Forward(const glm::vec3& euler) {
		return glm::normalize(glm::vec3(EulerToMatrix(euler) * glm::vec4(0, 0, -1, 0.0f)));
	}

	inline glm::vec3 Right(const glm::vec3& euler) {
		return glm::normalize(glm::cross(Forward(euler), glm::vec3(0, 1, 0)));
	}

	inline glm::vec3 Translate(const glm::vec3& pos,
							   const glm::vec3& localOffset,
							   const glm::vec3& euler) {
		glm::vec3 worldOffset = glm::vec3(EulerToMatrix(euler) * glm::vec4(localOffset, 0.0f));
		return pos + worldOffset;
	}

	inline glm::vec3 CalcLookAt(const glm::vec3& pos, const glm::vec3& euler) {
		return pos + Forward(euler);
	}
	
	inline std::optional<float> RayIntersectsTriangle(const Ray& ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float epsilon = 1e-6f) {
		glm::vec3 edge1 = v1 - v0;
		glm::vec3 edge2 = v2 - v0;
	
		glm::vec3 h = glm::cross(ray.direction, edge2);
		float a = glm::dot(edge1, h);
	
		if (glm::abs(a) < epsilon)
			return std::nullopt; // This ray is parallel to this triangle.
	
		float f = 1.0f / a;
		glm::vec3 s = ray.origin - v0;
		float u = f * glm::dot(s, h);
	
		if (u < 0.0f || u > 1.0f)
			return std::nullopt;
	
		glm::vec3 q = glm::cross(s, edge1);
		float v = f * glm::dot(ray.direction, q);
	
		if (v < 0.0f || u + v > 1.0f)
			return std::nullopt;
	
		// At this stage we can compute t to find out where the intersection point is on the line.
		float t = f * glm::dot(edge2, q);
	
		if (t > epsilon) // ray intersection
			return t;
	
		// This means that there is a line intersection but not a ray intersection.
		return std::nullopt;
	}
	
	inline std::optional<float> RayCast(Ray ray, const std::vector<std::shared_ptr<Mesh>>& meish) { // meish is the plural of mesh
		std::optional<float> closestHit;
		
		for (int indx = 0; indx < meish.size(); indx++) {
			const auto& mesh = meish[indx];
			
			if (!mesh->is_castable){
				continue;
			}
			
			glm::mat4 inverseTransform = glm::inverse(mesh->transform.ModelMatrix());
			glm::vec3 localOrigin = glm::vec3(inverseTransform * glm::vec4(ray.origin, 1.0));
			glm::vec3 localDirection = glm::normalize(glm::vec3(inverseTransform * glm::vec4(ray.direction, 0.0)));
			Ray localRay{ localOrigin, localDirection };
			
			for (size_t i = 0; i < mesh->indices.size(); i += 3) {
				const glm::vec3& v0 = mesh->vertices[mesh->indices[i]].position;
				const glm::vec3& v1 = mesh->vertices[mesh->indices[i + 1]].position;
				const glm::vec3& v2 = mesh->vertices[mesh->indices[i + 2]].position;
	
				if (auto t = RayIntersectsTriangle(localRay, v0, v1, v2)) {
					if (!closestHit || *t < *closestHit) {
						closestHit = *t;
					}
				}
			}
		}
	
		if (closestHit) {
			return *closestHit; // Distance from ray.origin in world space
		}
	
		return std::nullopt; // No hits
	}
}
