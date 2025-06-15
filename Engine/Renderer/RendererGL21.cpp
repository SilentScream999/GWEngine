#include "RendererGL21.h"
#include "../Core/Logger.h"
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Mesh.h"

static GLFWwindow* window      = nullptr;
static int        winWidth     = 800;
static int        winHeight    = 600;
static float      angle        = 0.0f;
static double     lastTime     = 0.0;

// -- new state for mouse capture and click detection --
static bool  glfwMouseCaptured = true;  // start captured
static bool  leftWasDown       = false;

static void FramebufferSizeCallback(GLFWwindow* wnd, int width, int height) {
	winWidth  = width;
	winHeight = height;
	glViewport(0, 0, width, height);
}

bool Renderer::RendererGL21::SetMeshes(std::vector<std::shared_ptr<Mesh>> msh) {
	meshes = msh;
	return true;
}

bool Renderer::RendererGL21::Init(Camera* c, Runtime::Runtime *r) {
	runtime = r;
	cam = c;

	if (!glfwInit()) {
		Logger::Error("Failed to initialize GLFW.");
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_SAMPLES, 4);

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

	glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
	glViewport(0, 0, winWidth, winHeight);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	glCullFace(GL_FRONT);

	glClearColor(
		20 / 255.0f,
		20 / 255.0f,
		50 / 255.0f, // oh this is the ugly background color!
		1.0f
	);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);

	GLfloat globalAmbient[4] = { 0, 0, 0, 1 };
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

	GLfloat lightAmb[4] = { 1, 1, 1, 1 };
	GLfloat lightDif[4] = { 1, 1, 1, 1 };
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDif);

	GLfloat lightDir[4] = { -1, -1, -1, 0 };
	glLightfv(GL_LIGHT0, GL_POSITION, lightDir);

	GLfloat matAmb[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	GLfloat matDif[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glMaterialfv(GL_FRONT, GL_AMBIENT, matAmb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, matDif);

	lastTime = glfwGetTime();

	// Hide & capture mouse at start
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwMouseCaptured = true;
	leftWasDown       = false;

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

	// --- handle Esc to unlock cursor ---
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && glfwMouseCaptured) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwMouseCaptured = false;
	}

	// --- handle left-click to re-lock cursor ---
	bool leftDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
	if (leftDown && !leftWasDown && !glfwMouseCaptured) {
		// center in window coords
		double cx = winWidth  * 0.5;
		double cy = winHeight * 0.5;
		glfwSetCursorPos(window, cx, cy);

		// hide & disable cursor
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwMouseCaptured = true;
	}
	leftWasDown = leftDown;

	// --- time & dt ---
	double now = glfwGetTime();
	float  dt  = static_cast<float>(now - lastTime);
	lastTime   = now;

	// --- only process camera input if we currently have capture ---
	if (glfwMouseCaptured) {
		//cam->ProcessInput(window, dt);
		runtime->ProcessInput(window, dt);
	}
	cam->updateForFrame();

	// --- render scene ---
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	{
		float aspect = float(winWidth) / float(winHeight);
		glm::mat4 proj = glm::perspective(
			glm::radians(60.0f), aspect, 0.1f, 100.0f
		);
		// match your original “depth-hack”
		proj[2][2] = proj[2][2] * 0.5f + proj[3][2] * 0.5f;
		proj[3][2] = proj[3][2] * 0.5f;

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(glm::value_ptr(proj));
	}
	GLfloat lightDir[4] = { -1.0f, -1.0f, -1.0f, 0.0f }; // directional light
	glLightfv(GL_LIGHT0, GL_POSITION, lightDir);

	glm::mat4 view = glm::lookAt(
		cam->position,
		cam->lookAtPosition,
		glm::vec3(0, 1, 0) // up vector
	);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(glm::value_ptr(view));
	glPushMatrix();

	glBegin(GL_TRIANGLES);
	for (const auto& mesh : meshes) {
		for (const auto& v : mesh->vertices) {
			glNormal3f(v.normal.x, v.normal.y, v.normal.z);
			glVertex3f(
				v.position.x + mesh->position.x,
				v.position.y + mesh->position.y,
				v.position.z + mesh->position.z
			);
		}
	}
	glEnd();
	glPopMatrix();

	glfwSwapBuffers(window);
	glfwPollEvents();
}