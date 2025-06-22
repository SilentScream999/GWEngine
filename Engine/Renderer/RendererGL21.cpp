// RendererGL21.cpp
#include "RendererGL21.h"
#include "../Core/Editor.h"      // <<< pull in the EditorRuntime definition
#include "../Core/Logger.h"
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#ifndef PI
  #define PI 3.14159265358979323846f
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Mesh.h"
#include "../Core/stb_impl.h"

// globals for our window and state
static GLFWwindow* window      = nullptr;
static int        winWidth     = 800;
static int        winHeight    = 600;
static float      angle        = 0.0f;
static double     lastTime     = 0.0;

// mouse capture state
static bool  glfwMouseCaptured = true;
static bool  leftWasDown       = false;

// skybox globals
static GLuint skyboxTexture = 0;

static void FramebufferSizeCallback(GLFWwindow* wnd, int width, int height) {
	winWidth  = width;
	winHeight = height;
	glViewport(0, 0, width, height);
}

// -----------------------------------------------------------------------------
// at top of RendererGL21.cpp, after your #includes:
// -----------------------------------------------------------------------------

// tesselation detail
static const int ARROW_SEGMENTS = 16;

// draw a unit cylinder along +Z from z=0..1, radius=1
static void DrawUnitCylinder()
{
    for(int i = 0; i < ARROW_SEGMENTS; ++i) {
        float a0 = 2.0f * PI * (float)i       / ARROW_SEGMENTS;
        float a1 = 2.0f * PI * (float)(i + 1) / ARROW_SEGMENTS;
        float x0 = cosf(a0), y0 = sinf(a0);
        float x1 = cosf(a1), y1 = sinf(a1);
        // side quad (two triangles):
        glBegin(GL_TRIANGLES);
          // first tri
          glNormal3f(x0, y0, 0);
          glVertex3f(x0, y0, 0);
          glVertex3f(x0, y0, 1);
          glVertex3f(x1, y1, 1);
          // second tri
          glNormal3f(x0, y0, 0);
          glVertex3f(x0, y0, 0);
          glVertex3f(x1, y1, 1);
          glVertex3f(x1, y1, 0);
        glEnd();
    }
}

// draw a unit cone along +Z from z=0..1, base radius=1 at z=0, tip at z=1
static void DrawUnitCone()
{
    glm::vec3 tip(0,0,1.0f);
    for(int i = 0; i < ARROW_SEGMENTS; ++i) {
        float a0 = 2.0f * PI * (float)i       / ARROW_SEGMENTS;
        float a1 = 2.0f * PI * (float)(i + 1) / ARROW_SEGMENTS;
        float x0 = cosf(a0), y0 = sinf(a0);
        float x1 = cosf(a1), y1 = sinf(a1);
        // single triangle per segment
        // compute normals by cross-product:
        glm::vec3 p0(x0, y0, 0.0f), p1(x1, y1, 0.0f);
        glm::vec3 edge0 = p0 - tip;
        glm::vec3 edge1 = p1 - tip;
        glm::vec3 n = glm::normalize(glm::cross(edge1, edge0));
        glBegin(GL_TRIANGLES);
          glNormal3f(n.x, n.y, n.z);
          glVertex3f(p0.x, p0.y, p0.z);
          glVertex3f(p1.x, p1.y, p1.z);
          glVertex3f(tip.x, tip.y, tip.z);
        glEnd();
    }
}

// draw an arrow at origin, pointing along +Z.
//  shaftLength = total length - headLength
//  shaftRadius, headRadius, headLength in same units.
static void DrawUnitArrow(float shaftRadius, float headRadius, float headLength, float totalLength)
{
    float shaftLength = totalLength - headLength;

    // --- draw shaft ---
    glPushMatrix();
      // scale X/Y by radius, Z by length
      glScalef(shaftRadius, shaftRadius, shaftLength);
      DrawUnitCylinder();
    glPopMatrix();

    // --- draw head ---
    glPushMatrix();
      // translate to end of shaft
      glTranslatef(0, 0, shaftLength);
      // scale X/Y by headRadius, Z by headLength
      glScalef(headRadius, headRadius, headLength);
      DrawUnitCone();
    glPopMatrix();
}

// orient & draw arrow from 'start' in 'dir' direction
static void DrawArrow(const glm::vec3& start, 
                      const glm::vec3& dir, 
                      float shaftRadius, 
                      float headRadius, 
                      float headLength)
{
    float totalLength = glm::length(dir);
    if (totalLength < 1e-6f) return;

    // compute rotation: align local +Z to dir
    glm::vec3 axis = glm::cross(glm::vec3(0,0,1), dir);
    float angle = acosf(glm::dot(glm::normalize(dir), glm::vec3(0,0,1)));
    
    glPushMatrix();
      glTranslatef(start.x, start.y, start.z);
      if (glm::length(axis) > 1e-6f)
        glRotatef(glm::degrees(angle), axis.x, axis.y, axis.z);
      // draw the unit arrow along +Z
      DrawUnitArrow(shaftRadius, headRadius, headLength, totalLength);
    glPopMatrix();
}

// helper: draw X/Y/Z arrows at mesh origin
static void DrawArrowGizmos(const glm::vec3& origin, float scale=1.0f)
{
    float shaftR = 0.02f * scale;
    float headR  = 0.06f * scale;
    float headL  = 0.2f  * scale;
    // X = red
    glColor3f(1,0,0);
    DrawArrow(origin, glm::vec3(scale,0,0), shaftR, headR, headL);
    // Y = green
    glColor3f(0,1,0);
    DrawArrow(origin, glm::vec3(0,scale,0), shaftR, headR, headL);
    // Z = blue
    glColor3f(0,0,1);
    DrawArrow(origin, glm::vec3(0,0,scale), shaftR, headR, headL);
}


// Simple skybox cube vertices (positions only, we'll use them as texture coords)
// Skybox vertices with UV coordinates (x, y, z, u, v)
// The numbers here have been adjusted to ensure they fit the texture
static float skyboxVertices[] = {
	// Front face (negative Z),
	-1.0,   -1.0,   -1.0,   0.25,   0.6666666666666666,
	1.0,    -1.0,   -1.0,   0.5,    0.6666666666666666,
	1.0,    1.0,    -1.0,   0.5,    0.3333333333333333,
	1.0,    1.0,    -1.0,   0.5,    0.3333333333333333,
	-1.0,   1.0,    -1.0,   0.25,   0.3333333333333333,
	-1.0,   -1.0,   -1.0,   0.25,   0.6666666666666666,
	
	
	// Back face (positive Z),
	-1.0,   -1.0,   1.0,    0.999,    0.665,
	1.0,    -1.0,   1.0,    0.75,     0.665,
	1.0,    1.0,    1.0,    0.75,     0.334,
	1.0,    1.0,    1.0,    0.75,     0.334,
	-1.0,   1.0,    1.0,    0.999,    0.334,
	-1.0,   -1.0,   1.0,    0.999,    0.665,
	
	
	// Left face (negative X),
	-1.0,   1.0,    1.0,    0.001,   0.334,
	-1.0,   1.0,    -1.0,   0.25,   0.334,
	-1.0,   -1.0,   -1.0,   0.25,   0.665,
	-1.0,   -1.0,   -1.0,   0.25,   0.665,
	-1.0,   -1.0,   1.0,    0.001,   0.665,
	-1.0,   1.0,    1.0,    0.001,   0.334,
	
	
	// Right face (positive X),
	1.0,    1.0,    1.0,    0.75,   0.334,
	1.0,    1.0,    -1.0,   0.5,    0.334,
	1.0,    -1.0,   -1.0,   0.5,    0.665,
	1.0,    -1.0,   -1.0,   0.5,    0.665,
	1.0,    -1.0,   1.0,    0.75,   0.665,
	1.0,    1.0,    1.0,    0.75,   0.334,
	
	
	// Bottom face (negative Y),
	-1.0,   -1.0,   -1.0,   0.251,   0.6666666666666666,
	1.0,    -1.0,   -1.0,   0.499,   0.6666666666666666,
	1.0,    -1.0,   1.0,    0.499,   0.999,
	1.0,    -1.0,   1.0,    0.499,   0.999,
	-1.0,   -1.0,   1.0,    0.251,   0.999,
	-1.0,   -1.0,   -1.0,   0.251,   0.6666666666666666,
	
	
	// Top face (positive Y),
	-1.0,   1.0,    -1.0,   0.251,   0.3333333333333333,
	1.0,    1.0,    -1.0,   0.499,   0.3333333333333333,
	1.0,    1.0,    1.0,    0.499,   0.001,
	1.0,    1.0,    1.0,    0.499,   0.001,
	-1.0,   1.0,    1.0,    0.251,   0.001,
	-1.0,   1.0,    -1.0,   0.251,   0.3333333333333333,
};

// Load skybox texture from bitmap file
void Renderer::RendererGL21::CreateSkyboxTexture(const char* filename) {
	int width, height, channels;
	unsigned char* data = stb_impl::LoadImageFromFile(filename, &width, &height, &channels);
	
	if (!data) {
		printf("Failed to load skybox texture, creating default texture\n");
		// Create a simple default texture if loading fails
		const int size = 256;
		data = new unsigned char[size * size * channels];
		width = height = size;
		
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				int idx = (y * size + x) * channels;
				float t = (float)y / (float)size;
				data[idx + 0] = (unsigned char)(134 + (206 - 134) * t);
				data[idx + 1] = (unsigned char)(206 + (234 - 206) * t);
				data[idx + 2] = (unsigned char)(234 + (255 - 234) * t);
			}
		}
	}

	glGenTextures(1, &skyboxTexture);
	glBindTexture(GL_TEXTURE_2D, skyboxTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	
	
	// sample colors from the skybox for lighting
	
	float xPoints[12] = {
		// horizontals
		0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0,
		// top - no bottom
		0.38
	};
	float yPoints[12] = {
		// horizontals
		0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5,
		// top - no bottom
		0.17
	};
	
	float totalr = 0.0;
	float totalg = 0.0;
	float totalb = 0.0;
	
	float totalstrength = 0.0;
	
	for (int i = 0; i < 12; i++) {
		float x = xPoints[i];
		float y = yPoints[i];
	
		float strength = (1.0-y)*(1.0-y);
		totalstrength += strength;
	
		// Convert UV to pixel coordinates
		int pixelX = (int)(x * (width - 1));
		int pixelY = (int)(y * (height - 1));
		int srcIndex = (pixelY * width + pixelX) * channels;
		
		totalr += data[srcIndex] * strength;
		totalg += data[srcIndex+1] * strength;
		totalb += data[srcIndex+2] * strength;
	}
	
	skybox_r = (totalr/totalstrength)/255.0f;
	skybox_g = (totalg/totalstrength)/255.0f;
	skybox_b = (totalb/totalstrength)/255.0f;
	
	delete[] data;
}

static void RenderSkybox() {
	// Save current OpenGL state
	glPushMatrix();
	glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);

	// Disable depth writing but keep depth testing
	glDepthMask(GL_FALSE);

	// Enable texturing and disable lighting
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	
	// Bind skybox texture
	glBindTexture(GL_TEXTURE_2D, skyboxTexture);
	
	// Set color to white so texture shows properly
	glColor3f(1.0f, 1.0f, 1.0f);

	// Render skybox cube with texture coordinates
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < 36; i++) {
		float x = skyboxVertices[i * 5 + 0];
		float y = skyboxVertices[i * 5 + 1];
		float z = skyboxVertices[i * 5 + 2];
		float u = skyboxVertices[i * 5 + 3];
		float v = skyboxVertices[i * 5 + 4];

		glTexCoord2f(u, v);
		glVertex3f(x, y, z);
	}
	glEnd();

	// Restore OpenGL state
	glPopAttrib();
	glPopMatrix();
	glDepthMask(GL_TRUE);
}

bool Renderer::RendererGL21::SetMeshes(std::vector<std::shared_ptr<Mesh>> msh) {
	meshes = std::move(msh);
	return true;
}

bool Renderer::RendererGL21::UpdateMesh(int indx, std::shared_ptr<Mesh> msh){
	if (indx >= meshes.size()) {
		meshes.resize(indx+1);
	}
	meshes[indx] = msh;
	return true;
}

int Renderer::RendererGL21::AddMesh(std::shared_ptr<Mesh> mesh) {
	for (int i = 0; i < meshes.size(); i++) {
		if (!meshes[i]) {
			UpdateMesh(i, mesh);
			return i;
		}
	}
	
	int indx = meshes.size();
	UpdateMesh(indx, mesh);
	return indx;
}

bool Renderer::RendererGL21::DeleteMesh(int indx) {
	if (indx < 0 || indx >= meshes.size()) {
		return false;
	}
	
	meshes[indx] = nullptr;
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

	// Create skybox texture
	CreateSkyboxTexture("assets/textures/skybox.bmp");
	// That has to happen before we assign the ambients

	GLfloat globalAmbient[4] = {1,1,1,1};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

	GLfloat lightAmb[4] = {skybox_r,skybox_g,skybox_b,1};
	GLfloat lightDif[4] = {skybox_r,skybox_g,skybox_b,1};
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDif);

	GLfloat lightDir[4] = {0,1,0,0};
	glLightfv(GL_LIGHT0, GL_POSITION, lightDir);

	GLfloat matAmb[4] = {skybox_r*.5f, skybox_g*.5f, skybox_b*.5f, 1.0f};
	GLfloat matDif[4] = {1,1,1,1};
	glMaterialfv(GL_FRONT, GL_AMBIENT, matAmb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, matDif);

	// hide & capture mouse by default
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwMouseCaptured = true;
	leftWasDown       = false;

	lastTime = glfwGetTime();
	Logger::Info("GLFW window, context, lighting, skybox, and MSAA initialized.");
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
	GLfloat lightDirFrame[4] = {0.0f, 1.0f, 0.0f, 0.0f};
	glLightfv(GL_LIGHT0, GL_POSITION, lightDirFrame);

	// camera/view
	glm::mat4 view = glm::lookAt(
		cam->transform.position,
		cam->lookAtPosition,
		glm::vec3(0,1,0)
	);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(glm::value_ptr(view));

	// *** RENDER SKYBOX FIRST ***
	// Remove translation from view matrix for skybox (keep only rotation)
	glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
	glLoadMatrixf(glm::value_ptr(skyboxView));
	RenderSkybox();
	
	// Restore full view matrix for regular geometry
	glLoadMatrixf(glm::value_ptr(view));

	// draw meshes
	for (const auto& mesh : meshes) {
		if (!mesh) {
			continue;
		}
		
		glPushMatrix();
		glTranslatef(mesh->transform.position.x, mesh->transform.position.y, mesh->transform.position.z);
		
		glRotatef(mesh->transform.rotation.y, 0.0f, 1.0f, 0.0f); // Yaw (Y-axis)
		glRotatef(mesh->transform.rotation.x, 1.0f, 0.0f, 0.0f); // Pitch (X-axis)
		glRotatef(mesh->transform.rotation.z, 0.0f, 0.0f, 1.0f); // Roll (Z-axis)
		
		glBegin(GL_TRIANGLES);
		
		for (const auto& v : mesh->vertices) {
		  glNormal3f(v.normal.x, v.normal.y, v.normal.z);
		  glVertex3f(v.position.x, v.position.y, v.position.z);
		}
		
		glEnd();
		
		glPopMatrix();

		// Only draw arrows if we're in the Editor
		if (dynamic_cast<Runtime::EditorRuntime*>(runtime)) {
			// save all the GL state we'll tweak
			glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT | GL_POLYGON_BIT);

			// turn off depth testing & depth writes so arrows always win
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);

			// keep them bright & double-sided
			glDisable(GL_LIGHTING);
			glDisable(GL_CULL_FACE);

			// draw our colored axis arrows at the mesh origin
			DrawArrowGizmos(mesh->transform.position, /*scale=*/0.5f);

			// restore everything exactly as it was
			glPopAttrib();
		}
	}
	glfwSwapBuffers(window);
	glfwPollEvents();
}