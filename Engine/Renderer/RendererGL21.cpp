// RendererGL21.cpp
#include "RendererGL21.h"
#include "../Core/Logger.h"
#include <GLFW/glfw3.h>
#include <GL/gl.h>

// GLM for matrix math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Mesh.h"

static GLFWwindow* window = nullptr;
static int        winWidth = 800;
static int        winHeight = 600;
static float      angle    = 0.0f;
static double     lastTime = 0.0;

// Callback to adjust viewport on resize
static void FramebufferSizeCallback(GLFWwindow* wnd, int width, int height) {
    winWidth  = width;
    winHeight = height;
    glViewport(0, 0, width, height);
}

bool Renderer::RendererGL21::Init() {
    if (!glfwInit()) {
        Logger::Error("Failed to initialize GLFW.");
        return false;
    }

    // Request OpenGL 2.1, resizable window + 4× MSAA
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);               // 4× MSAA

    window = glfwCreateWindow(winWidth, winHeight,
        "HL2-Style Engine - OpenGL 2.1", nullptr, nullptr);
    if (!window) {
        Logger::Error("Failed to create GLFW window.");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);                            // vsync

    // Set resize callback and initial viewport
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glViewport(0, 0, winWidth, winHeight);

    // Depth test + face culling
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glCullFace(GL_FRONT);
    // glEnable(GL_MULTISAMPLE);  ← remove this line

    // Clear colour
    glClearColor(20/255.0f, 20/255.0f, 50/255.0f, 1.0f);

    // === Lighting setup ===
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    // 1) Zero global ambient
    GLfloat globalAmbient[4] = { 0, 0, 0, 1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    // 2) Directional-light ambient & diffuse = 1.0
    GLfloat lightAmb[4] = { 1, 1, 1, 1 };
    GLfloat lightDif[4] = { 1, 1, 1, 1 };
    glLightfv(GL_LIGHT0, GL_AMBIENT,  lightAmb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightDif);

    // 3) Direction (DX9 style) = (1,1,1) → use (-1,-1,-1) in GL
    GLfloat lightDir[4] = { -1, -1, -1, 0 };
    glLightfv(GL_LIGHT0, GL_POSITION, lightDir);

    // 4) Material ambient = 0.1, diffuse = 1.0
    GLfloat matAmb[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    GLfloat matDif[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT,  matAmb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,  matDif);

    // Timer
    lastTime = glfwGetTime();

    // Load mesh…
    mesh = std::make_shared<Mesh>();
    if (!mesh->LoadFromOBJ("assets/models/test.obj")) {
        Logger::Error("Failed to load OBJ model.");
        return false;
    }

    Logger::Info("GLFW window, context, lighting, and MSAA initialized.");
    return true;
}

void Renderer::RendererGL21::RenderFrame() {
    if (!window) return;
    if (glfwWindowShouldClose(window)) {
        Logger::Info("GLFW window requested close; exiting.");
        glfwDestroyWindow(window);
        glfwTerminate();
        exit(0);
    }

    // Update rotation
    double now = glfwGetTime();
    float dt   = static_cast<float>(now - lastTime);
    lastTime   = now;
    angle     += glm::radians(45.0f) * dt;

    // Clear buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1) Projection with current aspect
    {
        float aspect = float(winWidth) / float(winHeight);
        glm::mat4 proj = glm::perspective(
            glm::radians(60.0f), aspect, 0.1f, 100.0f
        );
        // D3D-style Z remap
        proj[2][2] = proj[2][2]*0.5f + proj[3][2]*0.5f;
        proj[3][2] = proj[3][2]*0.5f;
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(glm::value_ptr(proj));
    }

    // 2) View & model
    glm::mat4 view = glm::lookAt(
        glm::vec3(0,2,5), glm::vec3(0,0,0), glm::vec3(0,1,0)
    );
    glm::mat4 model = glm::rotate(
        glm::mat4(1.0f), angle, glm::vec3(0,1,0)
    );

    // 3) Draw
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));
    glPushMatrix();
      glMultMatrixf(glm::value_ptr(model));
      glBegin(GL_TRIANGLES);
        for (const auto& v : mesh->vertices) {
            glNormal3f(v.normal.x, v.normal.y, v.normal.z);
            glVertex3f(v.position.x, v.position.y, v.position.z);
        }
      glEnd();
    glPopMatrix();

    // Present
    glfwSwapBuffers(window);
    glfwPollEvents();
}
