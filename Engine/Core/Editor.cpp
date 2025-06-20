// Editor.cpp
#include "Editor.h"
#include "EditorPanels.h"
#include "../Renderer/RendererManager.h"

#include <SDL.h>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <functional>

// Nuklear with SDL2 renderer
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_renderer.h"
#include "Logger.h"

// Global input state
static std::unordered_set<SDL_Scancode> pressedKeys;
static bool rightMouseHeld = false;

static SDL_Window   *win      = nullptr;
static SDL_Renderer *renderer = nullptr;
nk_context   *ctx      = nullptr;

// Initialization
bool Runtime::EditorRuntime::Init() {
	std::vector<std::shared_ptr<Mesh>> meshes;
	auto mesh1 = std::make_shared<Mesh>();
	if (!mesh1->LoadFromOBJ("assets/models/test2.obj")) {
		Logger::Error("Failed to load OBJ test2.obj");
		return false;
	}
	mesh1->transform.position = glm::vec3(0.0f);
	meshes.push_back(mesh1);

	auto mesh2 = std::make_shared<Mesh>();
	if (!mesh2->LoadFromOBJ("assets/models/test.obj")) {
		Logger::Error("Failed to load OBJ test.obj");
		return false;
	}
	mesh2->transform.position = glm::vec3(1.0f);
	meshes.push_back(mesh2);

	Renderer::RendererManager::SetMeshes(meshes);

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		Logger::Error(std::string("SDL_Init failed: ") + SDL_GetError());
		return false;
	}

	win = SDL_CreateWindow(
		"Nuclear GUI",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		1200, 800,
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
	);
	if (!win) {
		Logger::Error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
		return false;
	}

	renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		Logger::Error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
		return false;
	}

	ctx = nk_sdl_init(win, renderer);
	nk_font_atlas *atlas;
	nk_sdl_font_stash_begin(&atlas);
	nk_sdl_font_stash_end();

	return true;
}

// Prepare for frame rendering and handle input
void Runtime::EditorRuntime::PrepareForFrameRender() {
	static float ratio_x = 0.75f, ratio_y = 0.75f, right_ratio = 0.5f;
	static bool dragging_vsplit = false,
				dragging_hsplit = false,
				dragging_rhsplit = false;
	const int menu_height = 30, min_side = 100, min_top = 50, min_bot = 50;

	static float lastX = 0.0f, lastY = 0.0f;
	static bool firstMouse = true;
	static Uint32 lastTime = SDL_GetTicks();

	SDL_Event evt;
	nk_input_begin(ctx);

	int win_w, win_h;
	SDL_GetWindowSize(win, &win_w, &win_h);
	int content_h = std::max(100, win_h - menu_height);

	int drag_x = std::clamp(int(ratio_x * win_w), min_side, win_w - min_side);
	int drag_y = std::clamp(menu_height + int(ratio_y * content_h), menu_height + min_top, win_h - min_bot);
	int right_split_y = std::clamp(menu_height + int(right_ratio * content_h), menu_height + min_top, menu_height + content_h - min_bot);

	while (SDL_PollEvent(&evt)) {
		nk_sdl_handle_event(&evt);
		if (evt.type == SDL_QUIT) { Cleanup(); std::exit(0); }
		else if (evt.type == SDL_MOUSEBUTTONDOWN) {
			if (evt.button.button == SDL_BUTTON_LEFT) {
				int mx = evt.button.x, my = evt.button.y;
				dragging_vsplit = (std::abs(mx - drag_x) <= 5) && (my >= menu_height && my <= win_h);
				dragging_hsplit = (std::abs(my - drag_y) <= 5) && (mx >= 0 && mx <= drag_x);
				dragging_rhsplit = (mx >= drag_x) && (std::abs(my - right_split_y) <= 5);
			} else if (evt.button.button == SDL_BUTTON_RIGHT) {
				rightMouseHeld = true;
				firstMouse = true;
			}
		}
		else if (evt.type == SDL_MOUSEBUTTONUP) {
			if (evt.button.button == SDL_BUTTON_LEFT)
				dragging_vsplit = dragging_hsplit = dragging_rhsplit = false;
			else if (evt.button.button == SDL_BUTTON_RIGHT)
				rightMouseHeld = false;
		}
		else if (evt.type == SDL_KEYDOWN) {
			pressedKeys.insert(evt.key.keysym.scancode);
		}
		else if (evt.type == SDL_KEYUP) {
			pressedKeys.erase(evt.key.keysym.scancode);
		}
	}
	nk_input_end(ctx);

	if (dragging_vsplit || dragging_hsplit || dragging_rhsplit) {
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		if (dragging_vsplit)
			ratio_x = std::clamp(mx / float(win_w), 0.0f, 1.0f);
		if (dragging_hsplit)
			ratio_y = std::clamp((my - menu_height) / float(content_h), 0.0f, 1.0f);
		if (dragging_rhsplit)
			right_ratio = std::clamp((my - menu_height) / float(content_h), 0.0f, 1.0f);
		drag_x = std::clamp(int(ratio_x * win_w), min_side, win_w - min_side);
		content_h = win_h - menu_height;
		drag_y = std::clamp(menu_height + int(ratio_y * content_h), menu_height + min_top, win_h - min_bot);
		right_split_y = std::clamp(menu_height + int(right_ratio * content_h), menu_height + min_top, menu_height + content_h - min_bot);
	}

	 // Figure out if the cursor is inside the GL frame
	int mx, my;
	SDL_GetMouseState(&mx, &my);
	bool inGLFrame = (mx >= 0 && mx < drag_x)
				  && (my >= menu_height && my < drag_y);

	// Prepare timing
	Uint32 currentTime = SDL_GetTicks();
	float dt = (currentTime - lastTime) / 1000.0f;
	lastTime = currentTime;

	// Initialize mouse deltas
	float xoffset = 0.0f, yoffset = 0.0f;

	if (inGLFrame) {
		// — Mouse-look (right button)
		if (rightMouseHeld) {
			float xpos = float(mx), ypos = float(my);
			if (firstMouse) {
				lastX = xpos;
				lastY = ypos;
				firstMouse = false;
			} else {
				xoffset = lastX - xpos;
				yoffset = lastY - ypos;
				lastX = xpos;
				lastY = ypos;
			}
		} else {
			firstMouse = true; // reset when button is up
		}

		// — Keyboard (WASDQE)
		const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
		auto KeyIsDownGLFW = [&](int key)->bool {
			switch(key) {
				case GLFW_KEY_W: return keyboardState[SDL_SCANCODE_W];
				case GLFW_KEY_S: return keyboardState[SDL_SCANCODE_S];
				case GLFW_KEY_A: return keyboardState[SDL_SCANCODE_A];
				case GLFW_KEY_D: return keyboardState[SDL_SCANCODE_D];
				case GLFW_KEY_Q: return keyboardState[SDL_SCANCODE_Q];
				case GLFW_KEY_E: return keyboardState[SDL_SCANCODE_E];
				default: return false;
			}
		};

		// Feed both into the camera
		ProcessInput(xoffset, yoffset, KeyIsDownGLFW, dt);
	} else {
		// Outside the frame: reset mouse state so you don't jump on re-entry
		firstMouse = true;
	}

	// Capture & display frame
	int top_h = std::max(1, drag_y - menu_height);
	auto frameData = Renderer::RendererManager::rendererGL->CaptureFrame();
	if (frameData.width != drag_x || frameData.height != top_h) {
		Renderer::RendererManager::rendererGL->setSize(drag_x, top_h);
		frameData = Renderer::RendererManager::rendererGL->CaptureFrame();
	}
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom((void*)frameData.pixels.data(), frameData.width, frameData.height, 32, frameData.width*4,
												   0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
	struct nk_image nk_img = nk_image_ptr(texture);

	// Draw GUI
	SDL_SetRenderDrawColor(renderer,50,50,50,255);
	SDL_RenderClear(renderer);
	EditorPanels::DrawTopMenu(win_w, menu_height);
	EditorPanels::DrawImageView(nk_img, drag_x, menu_height, top_h);
	EditorPanels::DrawAssetBrowser(drag_x, drag_y, win_h);
	SDL_SetRenderDrawColor(renderer,100,100,100,255);
	SDL_RenderFillRect(renderer, &SDL_Rect{drag_x-1,menu_height,2,content_h});
	SDL_RenderFillRect(renderer, &SDL_Rect{0,drag_y-1,drag_x,2});
	int side_x = drag_x, side_w = win_w-drag_x;
	int Hierarchy_h = right_split_y-menu_height;
	int prop_h = content_h - Hierarchy_h;
	EditorPanels::DrawHierarchy(side_x,side_w,menu_height,Hierarchy_h);
	EditorPanels::DrawProperties(side_x,Hierarchy_h,menu_height,side_w,prop_h);
	SDL_RenderFillRect(renderer, &SDL_Rect{drag_x,right_split_y-1,side_w,2});
	nk_sdl_render(NK_ANTI_ALIASING_ON);
	SDL_RenderPresent(renderer);
	SDL_DestroyTexture(texture);
}

// ProcessInput overloads
void Runtime::EditorRuntime::ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float dt) {
	Renderer::RendererManager::cam->ProcessInput(xpos,ypos,KeyIsDown,dt);
}

void Runtime::EditorRuntime::ProcessInput(GLFWwindow* window, float dt) {
	Renderer::RendererManager::cam->ProcessInput(window,dt);
}

// Cleanup
void Runtime::EditorRuntime::Cleanup() {
	nk_sdl_shutdown();
	if (renderer) SDL_DestroyRenderer(renderer);
	if (win) SDL_DestroyWindow(win);
	SDL_Quit();
}
