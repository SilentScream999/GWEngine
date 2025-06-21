// RendererGL21.cpp
#include "RendererGL21.h"
#include "../Core/Editor.h"      // <<< pull in the EditorRuntime definition
#include "../Core/Logger.h"
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Mesh.h"

// globals for our window and state
static GLFWwindow* window      = nullptr;
static int        winWidth     = 800;
static int        winHeight    = 600;
static float      angle        = 0.0f;
static double     lastTime     = 0.0;

// mouse capture state
static bool  glfwMouseCaptured = true;
static bool  leftWasDown       = false;

static void FramebufferSizeCallback(GLFWwindow* wnd, int width, int height) {
    winWidth  = width;
    winHeight = height;
    glViewport(0, 0, width, height);
}

bool Renderer::RendererGL21::SetMeshes(std::vector<std::shared_ptr<Mesh>> msh) {
    meshes = std::move(msh);
    return true;
}

bool Renderer::RendererGL21::Init(Camera* c, Runtime::Runtime *r) {
    runtime = r;
    cam     = c;

    // ——— 1) Detect Editor vs Play ———
    // We'll hide the window entirely if this is the EditorRuntime
    bool isEditor = (dynamic_cast<Runtime::EditorRuntime*>(runtime) != nullptr);

    // ——— 2) Init GLFW ———
    if (!glfwInit()) {
        Logger::Error("Failed to initialize GLFW.");
        return false;
    }

    // ——— 3) Window hints ———
    // Hide the window if in editor mode, otherwise show it
    glfwWindowHint(GLFW_VISIBLE, isEditor ? GLFW_FALSE : GLFW_TRUE);

    // OpenGL 2.1 context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

    // Window features
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES,   4);

    // ——— 4) Create window & context ———
    window = glfwCreateWindow(
        winWidth, winHeight,
        "HL2-Style Engine - OpenGL 2.1",
        nullptr, nullptr
    );
    if (!window) {
        Logger::Error("Failed to create GLFW window.");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // ——— 5) Setup callbacks & GL state ———
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glViewport(0, 0, winWidth, winHeight);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glCullFace(GL_FRONT);

    glClearColor(20/255.0f, 20/255.0f, 50/255.0f, 1.0f);

    // Lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    GLfloat globalAmbient[4] = {0,0,0,1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    GLfloat lightAmb[4] = {1,1,1,1};
    GLfloat lightDif[4] = {1,1,1,1};
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDif);

    GLfloat lightDir[4] = {-1,-1,-1,0};
    glLightfv(GL_LIGHT0, GL_POSITION, lightDir);

    GLfloat matAmb[4] = {0.1f,0.1f,0.1f,1.0f};
    GLfloat matDif[4] = {1,1,1,1};
    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDif);

    // hide & capture mouse by default
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwMouseCaptured = true;
    leftWasDown       = false;

    lastTime = glfwGetTime();
    Logger::Info("GLFW window, context, lighting, and MSAA initialized.");
    return true;
}

Renderer::ImageData Renderer::RendererGL21::CaptureFrame() {
    ImageData data;
    data.width  = winWidth;
    data.height = winHeight;
    data.pixels.resize(winWidth * winHeight * 4);

    glReadPixels(0, 0, winWidth, winHeight, GL_RGBA, GL_UNSIGNED_BYTE, data.pixels.data());

    // flip vertically
    std::vector<unsigned char> flipped(winWidth * winHeight * 4);
    for (int y = 0; y < winHeight; ++y) {
        memcpy(
            &flipped[y * winWidth * 4],
            &data.pixels[(winHeight - 1 - y) * winWidth * 4],
            winWidth * 4
        );
    }
    data.pixels = std::move(flipped);
    return data;
}

void Renderer::RendererGL21::setSize(int newWidth, int newHeight) {
    glfwSetWindowSize(window, newWidth, newHeight);
}

void Renderer::RendererGL21::RenderFrame() {
    if (!window) return;

    if (glfwWindowShouldClose(window)) {
        Logger::Info("GLFW window requested close; exiting.");
        glfwDestroyWindow(window);
        glfwTerminate();
        exit(0);
    }

    // ESC unlocks cursor
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && glfwMouseCaptured) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwMouseCaptured = false;
    }

    // Left-click to re-lock
    bool leftDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    if (leftDown && !leftWasDown && !glfwMouseCaptured) {
        double cx = winWidth * 0.5;
        double cy = winHeight * 0.5;
        glfwSetCursorPos(window, cx, cy);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwMouseCaptured = true;
    }
    leftWasDown = leftDown;

    // time delta
    double now = glfwGetTime();
    float  dt  = static_cast<float>(now - lastTime);
    lastTime   = now;

    // input only if captured
    if (glfwMouseCaptured) {
        runtime->ProcessInput(window, dt);
    }
    cam->updateForFrame();

    // clear & draw
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // setup projection
    float aspect = float(winWidth) / float(winHeight);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
    proj[2][2] = proj[2][2] * 0.5f + proj[3][2] * 0.5f;
    proj[3][2] = proj[3][2] * 0.5f;
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(proj));

    // reset light direction each frame
    GLfloat lightDirFrame[4] = {-1.0f, -1.0f, -1.0f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightDirFrame);

    // camera/view
    glm::mat4 view = glm::lookAt(
        cam->transform.position,
        cam->lookAtPosition,
        glm::vec3(0,1,0)
    );
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    // draw meshes
    glBegin(GL_TRIANGLES);
      for (const auto& mesh : meshes) {
        for (const auto& v : mesh->vertices) {
          glNormal3f(v.normal.x, v.normal.y, v.normal.z);
          glVertex3f(v.position.x + mesh->position.x,
                     v.position.y + mesh->position.y,
                     v.position.z + mesh->position.z);
        }
      }
    glEnd();

    glfwSwapBuffers(window);
    glfwPollEvents();
}
