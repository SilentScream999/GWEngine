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

// SDL and nuklear context
static SDL_Window *win;
static SDL_Renderer *renderer;
static struct nk_context *ctx;

bool Runtime::EditorRuntime::Init() {
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
	
	// Create your nuklear GUI
	if (nk_begin(ctx, "Hello Nuclear World!", nk_rect(50, 50, 400, 300),
				NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
				NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {
		
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
	// Input is handled automatically by nuklear SDL2 backend in PrepareForFrameRender
	// You can still do custom input handling here if needed
}

void Runtime::EditorRuntime::ProcessInput(GLFWwindow* window, float dt) {
	// Since we're using SDL2 now, you might want to remove GLFW dependency
	// or keep this for compatibility with your existing renderer
}

void Runtime::EditorRuntime::Cleanup() {
	nk_sdl_shutdown();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(win);
	SDL_Quit();
}