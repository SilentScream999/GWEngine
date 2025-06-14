#define GLM_ENABLE_EXPERIMENTAL
#include "Camera.h"
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h> // for GLFWwindow in ProcessInput

Camera::Camera() {
    // Initial position & rotation; adjust as desired
    position = glm::vec3(0.0f, 2.0f, 5.0f);
    rotation = glm::vec3(0.0f); // pitch=0, yaw=0
    firstMouse = true;
    lastX = lastY = 0.0;
}

void Camera::ProcessInput(GLFWwindow* window, float dt) {
    // Build rotation matrix from yaw (y), pitch (x), roll (z)
    glm::mat4 rotMatrix = glm::yawPitchRoll(rotation.y, rotation.x, rotation.z);
    glm::vec3 forward = glm::normalize(glm::vec3(rotMatrix * glm::vec4(0, 0, -1, 0)));
    glm::vec3 right   = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
    glm::vec3 up      = glm::vec3(0,1,0);

    // Keyboard: WASD for horizontal movement, Q/E for up/down
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        position += forward * movementSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        position -= forward * movementSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        position -= right   * movementSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        position += right   * movementSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        position -= up      * movementSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        position += up      * movementSpeed * dt;

    // Mouse: get cursor position
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = float(xpos - lastX);
    float yoffset = float(lastY - ypos); // reversed: moving up yields positive yoffset
    lastX = xpos;
    lastY = ypos;

    // Apply rotation: convert degrees to radians
    rotation.y += glm::radians(xoffset * mouseSensitivity); // yaw
    rotation.x += glm::radians(yoffset * mouseSensitivity); // pitch

    // Clamp pitch to avoid flipping
    float pitchLimit = glm::radians(89.0f);
    if (rotation.x >  pitchLimit) rotation.x =  pitchLimit;
    if (rotation.x < -pitchLimit) rotation.x = -pitchLimit;
}

void Camera::updateForFrame() {
    lookAtPosition = CalcLookAt(position, rotation);
}

glm::vec3 Camera::CalcLookAt(const glm::vec3& pos, const glm::vec3& rotEuler) {
    glm::mat4 rotMatrix = glm::yawPitchRoll(rotEuler.y, rotEuler.x, rotEuler.z);
    glm::vec3 forward = glm::vec3(rotMatrix * glm::vec4(0, 0, -1, 0));
    return pos + forward;
}
