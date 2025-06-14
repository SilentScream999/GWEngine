#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h> // for GLFWwindow* in ProcessInput

class Camera {
public:
    Camera();

    // Public state
    glm::vec3 position;
    glm::vec3 rotation;        // x = pitch, y = yaw, z = roll (unused)
    glm::vec3 lookAtPosition;  // computed each frame

    // Tuning parameters
    float movementSpeed    = 5.0f;   // units per second
    float mouseSensitivity = 0.1f;   // degrees per pixel

    // For GLFW mouse handling
    bool firstMouse;
    double lastX, lastY;

    // Call once per frame after input to update lookAtPosition
    void updateForFrame();

    // For GLFW/GL input path: call every frame with the GLFWwindow and deltaTime
    void ProcessInput(GLFWwindow* window, float deltaTime);

private:
    glm::vec3 CalcLookAt(const glm::vec3& position, const glm::vec3& rotationEulerRadians);
};
