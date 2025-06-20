#include "Editor.h"
#include "../Renderer/RendererManager.h"

// Nuklear with SDL2 renderer - much simpler!
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

// SDL and nuklear context
static SDL_Window *win;
static SDL_Renderer *renderer;
static struct nk_context *ctx;

bool Runtime::EditorRuntime::Init() {
	std::vector<std::shared_ptr<Mesh>> meshes;

	auto mesh1 = std::make_shared<Mesh>();
	if (!mesh1->LoadFromOBJ("assets/models/test2.obj")) {
		Logger::Error("Failed to load OBJ test2.obj");
		return false;
	}
	mesh1->position = glm::vec3(0.0f, 0.0f, 0.0f);
	meshes.push_back(mesh1);

	auto mesh2 = std::make_shared<Mesh>();
	if (!mesh2->LoadFromOBJ("assets/models/test.obj")) {
		Logger::Error("Failed to load OBJ test.obj");
		return false;
	}
	mesh2->position = glm::vec3(1.0f, 1.0f, 1.0f);
	meshes.push_back(mesh2);
	
	Renderer::RendererManager::SetMeshes(meshes);
	
	// Initialize SDL2
	SDL_Init(SDL_INIT_VIDEO);
	
	// Create window and renderer
	win = SDL_CreateWindow("Nuclear GUI",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		1200, 800, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
		
	renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	
	// Initialize nuklear with SDL2 renderer
	ctx = nk_sdl_init(win, renderer);
	
	// Load default font
	struct nk_font_atlas *atlas;
	nk_sdl_font_stash_begin(&atlas);
	nk_sdl_font_stash_end();
	
	return true;
}

void Runtime::EditorRuntime::PrepareForFrameRender() {
	// Handle SDL events
	SDL_Event evt;
	nk_input_begin(ctx);
	while (SDL_PollEvent(&evt)) {
		nk_sdl_handle_event(&evt);
		if (evt.type == SDL_QUIT) {
			// Handle quit
		}
	}
	nk_input_end(ctx);
	
	// Capture the frame
	Renderer::ImageData frameData = Renderer::RendererManager::rendererGL->CaptureFrame();
	
	// Create SDL texture from the pixel data
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
		frameData.pixels.data(), 
		frameData.width, frameData.height, 
		32, frameData.width * 4,
		0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
	);
	
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
	
	// Convert to Nuklear image
	struct nk_image nk_img = nk_image_ptr(texture);
	
	
	// Create your nuklear GUI
	
	/* This here is for if you want a movable/resizable/minimizable/titled/scalable view. Do with that what you will
	if (nk_begin(ctx, "View image!", nk_rect(150, 150, 400, 300),
				NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
				NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {
	*/
	
	int win_width, win_height;
	SDL_GetWindowSize(win, &win_width, &win_height);
	
	int imageend_x = win_width*.75;
	int imageend_y = win_height*.75;
	
	if (frameData.height != imageend_y || frameData.width != imageend_x) {
		Renderer::RendererManager::rendererGL->setSize(imageend_x, imageend_y);
	}
	
	int side_width = win_width-imageend_x;
	
	if (nk_begin(ctx, "Main image view", nk_rect(0, 0, imageend_x, imageend_y), 0)) { // the 0 on the end is just no modifiers
		nk_layout_row_static(ctx, imageend_y, imageend_x, 1); // it's height followed by width for some reason
		nk_image(ctx, nk_img);
	}
	nk_end(ctx);
	
	// Create your nuklear GUI
	if (nk_begin(ctx, "Side bar.", nk_rect(imageend_x, 0, side_width, win_height), NK_WINDOW_BORDER)) {
		nk_layout_row_static(ctx, 30, 200, 1);
		nk_label(ctx, "Hello from Nuklear + SDL2!", NK_TEXT_CENTERED);
		
		nk_layout_row_dynamic(ctx, 30, 2);
		if (nk_button_label(ctx, "Nuclear Button")) {
			printf("Nuclear button clicked!\n");
		}
		if (nk_button_label(ctx, "SDL2 Button")) {
			printf("SDL2 button clicked!\n");
		}
		
		// Simple slider
		static float value = 0.5f;
		nk_layout_row_dynamic(ctx, 30, 1);
		nk_slider_float(ctx, 0.0f, &value, 1.0f, 0.01f);
		
		// Text input
		static char text[256] = "Nuclear text...";
		nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, text, sizeof(text), nk_filter_default);
	}
	nk_end(ctx);
	
	// render the frame
	
	// Clear screen
	SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
	SDL_RenderClear(renderer);
	
	// Render nuklear
	nk_sdl_render(NK_ANTI_ALIASING_ON);
	
	// Present
	SDL_RenderPresent(renderer);
}

void Runtime::EditorRuntime::ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float dt) {
	Renderer::RendererManager::cam->ProcessInput(xpos, ypos, KeyIsDown, dt); // later consider replacing this with a 'move omnicient?'
}

void Runtime::EditorRuntime::ProcessInput(GLFWwindow* window, float dt) {
	Renderer::RendererManager::cam->ProcessInput(window, dt); // this should later also be a 'move omnicient'
}

void Runtime::EditorRuntime::Cleanup() {
	nk_sdl_shutdown();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(win);
	SDL_Quit();
}