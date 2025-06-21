// Editor.cpp
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

#include <SDL.h>      // for SDL_Init, SDL_Event, etc.
#include <cstdlib>    // for std::exit
#include <string>     // for std::string
#include <algorithm>  // for std::clamp

// SDL and nuklear context
static SDL_Window   *win      = nullptr;
static SDL_Renderer *renderer = nullptr;
static struct nk_context *ctx = nullptr;

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
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        Logger::Error(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    // Create window and renderer
    win = SDL_CreateWindow("Nuclear GUI",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1200, 800,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) {
        Logger::Error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        Logger::Error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
        return false;
    }

    // Initialize nuklear with SDL2 renderer
    ctx = nk_sdl_init(win, renderer);

    // Load default font
    struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);
    nk_sdl_font_stash_end();

    return true;
}

void Runtime::EditorRuntime::PrepareForFrameRender() {
    SDL_Event evt;
    nk_input_begin(ctx);

    static float ratio_x = 0.75f;
    static float ratio_y = 0.75f;
    static float right_ratio = 0.5f;
    static bool dragging_vsplit = false;
    static bool dragging_hsplit = false;
    static bool dragging_rhsplit = false;

    const int menu_height = 30;
    const int min_side = 100;
    const int min_top = 50;
    const int min_bot = 50;

    int win_w, win_h;
    SDL_GetWindowSize(win, &win_w, &win_h);
    int content_h = win_h - menu_height;
    if (content_h < 100) content_h = 100;

    int drag_x = int(ratio_x * win_w);
    drag_x = std::clamp(drag_x, min_side, win_w - min_side);

    int drag_y = menu_height + int(ratio_y * content_h);
    drag_y = std::clamp(drag_y, menu_height + min_top, win_h - min_bot);

    int right_split_y = menu_height + int(right_ratio * content_h);
    right_split_y = std::clamp(right_split_y, menu_height + min_top, menu_height + content_h - min_bot);

    while (SDL_PollEvent(&evt)) {
        nk_sdl_handle_event(&evt);

        if (evt.type == SDL_QUIT) {
            Cleanup();
            std::exit(0);
        } else if (evt.type == SDL_WINDOWEVENT && evt.window.event == SDL_WINDOWEVENT_RESIZED) {
        } else if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == SDL_BUTTON_LEFT) {
            int mx = evt.button.x;
            int my = evt.button.y;

            if (std::abs(mx - drag_x) <= 5 && my >= menu_height && my <= win_h) {
                dragging_vsplit = true;
            } else if (std::abs(my - drag_y) <= 5 && mx >= 0 && mx <= drag_x) {
                dragging_hsplit = true;
            } else if (mx >= drag_x && std::abs(my - right_split_y) <= 5) {
                dragging_rhsplit = true;
            }
        } else if (evt.type == SDL_MOUSEBUTTONUP && evt.button.button == SDL_BUTTON_LEFT) {
            dragging_vsplit = false;
            dragging_hsplit = false;
            dragging_rhsplit = false;
        }
    }

    nk_input_end(ctx);

    if (dragging_vsplit || dragging_hsplit || dragging_rhsplit) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (dragging_vsplit) {
            mx = std::clamp(mx, min_side, win_w - min_side);
            ratio_x = mx / float(win_w);
        }
        if (dragging_hsplit) {
            int rel = std::clamp(my - menu_height, min_top, content_h - min_bot);
            ratio_y = rel / float(content_h);
        }
        if (dragging_rhsplit) {
            int rel = std::clamp(my - menu_height, min_top, content_h - min_bot);
            right_ratio = rel / float(content_h);
        }

        drag_x = int(ratio_x * win_w);
        drag_x = std::clamp(drag_x, min_side, win_w - min_side);

        content_h = win_h - menu_height;

        drag_y = menu_height + int(ratio_y * content_h);
        drag_y = std::clamp(drag_y, menu_height + min_top, win_h - min_bot);

        right_split_y = menu_height + int(right_ratio * content_h);
        right_split_y = std::clamp(right_split_y, menu_height + min_top, menu_height + content_h - min_bot);
    }

    int top_h = drag_y - menu_height;
    if (top_h < 1) top_h = 1;

    Renderer::ImageData frameData = Renderer::RendererManager::rendererGL->CaptureFrame();
    if (frameData.width != drag_x || frameData.height != top_h) {
        Renderer::RendererManager::rendererGL->setSize(drag_x, top_h);
        frameData = Renderer::RendererManager::rendererGL->CaptureFrame();
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        (void*)frameData.pixels.data(),
        frameData.width, frameData.height,
        32, frameData.width * 4,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
    );
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    struct nk_image nk_img = nk_image_ptr(texture);

    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderClear(renderer);

    {
        struct nk_rect menu_rect = nk_rect(0, 0, (float)win_w, (float)menu_height);
        if (nk_begin(ctx, "TopMenu", menu_rect, NK_WINDOW_NO_SCROLLBAR)) {
            nk_layout_row_begin(ctx, NK_STATIC, menu_height, 3);
            nk_layout_row_push(ctx, 50.0f);
            if (nk_button_label(ctx, "File")) {}
            nk_layout_row_push(ctx, 50.0f);
            if (nk_button_label(ctx, "Edit")) {}
            nk_layout_row_push(ctx, 50.0f);
            if (nk_button_label(ctx, "Run")) {}
            nk_layout_row_end(ctx);
        }
        nk_end(ctx);
    }

    {
        struct nk_rect img_rect = nk_rect(0, (float)menu_height, (float)drag_x, (float)top_h);
        if (nk_begin(ctx, "Main image view", img_rect, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
            nk_layout_row_static(ctx, top_h, drag_x, 1);
            nk_image(ctx, nk_img);
        }
        nk_end(ctx);
    }

    {
        int asset_h = win_h - drag_y;
        struct nk_rect asset_rect = nk_rect(0, (float)drag_y, (float)drag_x, (float)asset_h);

        struct nk_vec2 old_padding = ctx->style.window.padding;
        ctx->style.window.padding.x = 10;

        if (nk_begin(ctx, "Asset Browser", asset_rect, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Asset 1", NK_TEXT_LEFT);
            nk_label(ctx, "Asset 2", NK_TEXT_LEFT);
            nk_label(ctx, "Asset 3", NK_TEXT_LEFT);
        }
        nk_end(ctx);

        ctx->style.window.padding = old_padding;
    }

    {
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect vr = { drag_x - 1, menu_height, 2, content_h };
        SDL_RenderFillRect(renderer, &vr);
    }

    {
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect hr = { 0, drag_y - 1, drag_x, 2 };
        SDL_RenderFillRect(renderer, &hr);
    }

    int side_x = drag_x;
    int side_w = win_w - drag_x;
    int sidebar_h = right_split_y - menu_height;
    int prop_h = content_h - sidebar_h;

    {
        struct nk_rect side_rect = nk_rect(
            (float)side_x,
            (float)menu_height,
            (float)side_w,
            (float)sidebar_h
        );
        if (nk_begin(ctx, "Side bar", side_rect, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
            nk_layout_row_static(ctx, 30, side_w - 20, 1);
            nk_label(ctx, "Hello from Nuklear + SDL2!", NK_TEXT_CENTERED);

            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_button_label(ctx, "Nuclear Button"))
                printf("Nuclear button clicked!\n");
            if (nk_button_label(ctx, "SDL2 Button"))
                printf("SDL2 button clicked!\n");

            static float value = 0.5f;
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_slider_float(ctx, 0.0f, &value, 1.0f, 0.01f);

            static char text[256] = "Nuclear text...";
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, text, sizeof(text), nk_filter_default);
        }
        nk_end(ctx);
    }

	// 8) Properties (small arrow buttons + full‑width editable field, 4 decimals)
	{
		struct nk_rect prop_rect = nk_rect(
			(float)side_x,
			(float)(menu_height + sidebar_h),
			(float)side_w,
			(float)prop_h
		);
		if (nk_begin(ctx, "Properties", prop_rect,
					NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
			static float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
			static char bufX[32], bufY[32], bufZ[32];
			static bool init = false;
			if (!init) {
				snprintf(bufX, sizeof(bufX), "%.4f", posX);
				snprintf(bufY, sizeof(bufY), "%.4f", posY);
				snprintf(bufZ, sizeof(bufZ), "%.4f", posZ);
				init = true;
			}

		auto draw_field = [&](const char* label, float& val, char* buf) {
		const float row_height = 22.0f;
		const float label_width = 80.0f;
		const float btn_width = 20.0f;
		const float field_width = 80.0f;

		nk_layout_row_begin(ctx, NK_STATIC, row_height, 4);

		// Label (left-aligned)
		nk_layout_row_push(ctx, label_width);
		nk_label(ctx, label, NK_TEXT_LEFT);

		// Minus button
		nk_layout_row_push(ctx, btn_width);
		if (nk_button_symbol(ctx, NK_SYMBOL_TRIANGLE_LEFT)) {
			val = std::clamp(val - 0.1f, -100.0f, 100.0f);
			snprintf(buf, 32, "%.4f", val);
		}

		// Input field
		nk_layout_row_push(ctx, field_width);
		if (nk_edit_string_zero_terminated(ctx,
				NK_EDIT_FIELD | NK_EDIT_CLIPBOARD,
				buf, 32, nk_filter_float)) {
			val = strtof(buf, nullptr);
			snprintf(buf, 32, "%.4f", val);
		}

		// Plus button
		nk_layout_row_push(ctx, btn_width);
		if (nk_button_symbol(ctx, NK_SYMBOL_TRIANGLE_RIGHT)) {
			val = std::clamp(val + 0.1f, -100.0f, 100.0f);
			snprintf(buf, 32, "%.4f", val);
		}

		nk_layout_row_end(ctx);
	};


			draw_field("Position X", posX, bufX);
			draw_field("Position Y", posY, bufY);
			draw_field("Position Z", posZ, bufZ);
		}
		nk_end(ctx);
	}

    {
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect rhr = { drag_x, right_split_y - 1, side_w, 2 };
        SDL_RenderFillRect(renderer, &rhr);
    }

    nk_sdl_render(NK_ANTI_ALIASING_ON);
    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(texture);
}

void Runtime::EditorRuntime::ProcessInput(float xpos, float ypos,
                                          std::function<bool(int)> KeyIsDown,
                                          float dt) {
    Renderer::RendererManager::cam->ProcessInput(xpos, ypos,
                                                 KeyIsDown, dt);
}

void Runtime::EditorRuntime::ProcessInput(GLFWwindow* window, float dt) {
    Renderer::RendererManager::cam->ProcessInput(window, dt);
}

void Runtime::EditorRuntime::Cleanup() {
    nk_sdl_shutdown();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (win)      SDL_DestroyWindow(win);
    SDL_Quit();
}
