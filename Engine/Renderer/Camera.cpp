#include "Camera.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "../Core/MathHelpers.h"

Camera::Camera() {
    transform.position = glm::vec3(0.0f, 2.0f, 5.0f);
    transform.rotation = glm::vec3(0.0f);
    firstMouse = true;
    lastX = 0.0;
    lastY = 0.0;
}

void Camera::ProcessInput(float xpos,
                          float ypos,
                          std::function<bool(int)> KeyIsDown,
                          float dt)
{
    float xoff = xpos * mouseSensitivity;
    float yoff = ypos * mouseSensitivity;

    transform.rotation.y += glm::radians(xoff);
    transform.rotation.x += glm::clamp(
        glm::radians(yoff),
        -glm::radians(89.0f),
         glm::radians(89.0f)
    );

    if (KeyIsDown('W')) transform.TranslateBy({0, 0, -movementSpeed * dt});
    if (KeyIsDown('S')) transform.TranslateBy({0, 0,  movementSpeed * dt});
    if (KeyIsDown('A')) transform.TranslateBy({-movementSpeed * dt, 0, 0});
    if (KeyIsDown('D')) transform.TranslateBy({ movementSpeed * dt, 0, 0});
    if (KeyIsDown('Q')) transform.TranslateBy({0, -movementSpeed * dt, 0});
    if (KeyIsDown('E')) transform.TranslateBy({0,  movementSpeed * dt, 0});
}

void Camera::ProcessInput(GLFWwindow* window, float dt)
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        transform.TranslateBy({0, 0, -movementSpeed * dt});
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        transform.TranslateBy({0, 0,  movementSpeed * dt});
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        transform.TranslateBy({-movementSpeed * dt, 0, 0});
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        transform.TranslateBy({ movementSpeed * dt, 0, 0});
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        transform.TranslateBy({0, -movementSpeed * dt, 0});
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        transform.TranslateBy({0,  movementSpeed * dt, 0});

    double xpos_d, ypos_d;
    glfwGetCursorPos(window, &xpos_d, &ypos_d);
    xpos_d *= -1.0;

    if (firstMouse) {
        lastX = xpos_d;
        lastY = ypos_d;
        firstMouse = false;
    }

    float xoffset = float(xpos_d - lastX);
    float yoffset = float(lastY - ypos_d);
    lastX = xpos_d;
    lastY = ypos_d;

    transform.rotation.y += glm::radians(xoffset * mouseSensitivity);
    transform.rotation.x += glm::radians(yoffset * mouseSensitivity);

    float pitchLimit = glm::radians(89.0f);
    transform.rotation.x = std::clamp(transform.rotation.x, -pitchLimit, pitchLimit);
}

void Camera::updateForFrame() {
    lookAtPosition = transform.position + transform.GetForward();
}

glm::vec3 Camera::CalcLookAt(const glm::vec3& pos, const glm::vec3& rotEuler) {
    return MathHelpers::CalcLookAt(pos, rotEuler);
}
