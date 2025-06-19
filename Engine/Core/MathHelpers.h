#pragma once
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

}
