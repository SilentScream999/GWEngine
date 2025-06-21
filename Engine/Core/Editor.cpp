// Editor.cpp
#include "Editor.h"
#include "EditorPanels.h"
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

#include <SDL.h>
#include <cstdlib>
#include <string>
#include <algorithm>

static SDL_Window   *win      = nullptr;
SDL_Renderer *renderer = nullptr;
nk_context *ctx = nullptr;

bool Runtime::EditorRuntime::Init() {
    std::vector<std::shared_ptr<Mesh>> meshes;
    auto mesh1 = std::make_shared<Mesh>();
    if (!mesh1->LoadFromOBJ("assets/models/test2.obj")) {
        Logger::Error("Failed to load OBJ test2.obj");
        return false;
    }
    mesh1->position = glm::vec3(0.0f);
    meshes.push_back(mesh1);

    auto mesh2 = std::make_shared<Mesh>();
    if (!mesh2->LoadFromOBJ("assets/models/test.obj")) {
        Logger::Error("Failed to load OBJ test.obj");
        return false;
    }
    mesh2->position = glm::vec3(1.0f);
    meshes.push_back(mesh2);

    Renderer::RendererManager::SetMeshes(meshes);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        Logger::Error(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    win = SDL_CreateWindow("Nuclear GUI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1200, 800, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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

void Runtime::EditorRuntime::PrepareForFrameRender() {
    SDL_Event evt;
    nk_input_begin(ctx);

    static float ratio_x = 0.75f, ratio_y = 0.75f, right_ratio = 0.5f;
    static bool dragging_vsplit = false, dragging_hsplit = false, dragging_rhsplit = false;

    const int menu_height = 30, min_side = 100, min_top = 50, min_bot = 50;

    int win_w, win_h;
    SDL_GetWindowSize(win, &win_w, &win_h);
    int content_h = std::max(100, win_h - menu_height);

    int drag_x = std::clamp(int(ratio_x * win_w), min_side, win_w - min_side);
    int drag_y = std::clamp(menu_height + int(ratio_y * content_h), menu_height + min_top, win_h - min_bot);
    int right_split_y = std::clamp(menu_height + int(right_ratio * content_h), menu_height + min_top, menu_height + content_h - min_bot);

    while (SDL_PollEvent(&evt)) {
        nk_sdl_handle_event(&evt);
        if (evt.type == SDL_QUIT) { Cleanup(); std::exit(0); }
        else if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == SDL_BUTTON_LEFT) {
            int mx = evt.button.x, my = evt.button.y;
            dragging_vsplit = std::abs(mx - drag_x) <= 5 && my >= menu_height && my <= win_h;
            dragging_hsplit = std::abs(my - drag_y) <= 5 && mx >= 0 && mx <= drag_x;
            dragging_rhsplit = mx >= drag_x && std::abs(my - right_split_y) <= 5;
        } else if (evt.type == SDL_MOUSEBUTTONUP && evt.button.button == SDL_BUTTON_LEFT) {
            dragging_vsplit = dragging_hsplit = dragging_rhsplit = false;
        }
    }
    nk_input_end(ctx);

    if (dragging_vsplit || dragging_hsplit || dragging_rhsplit) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (dragging_vsplit) ratio_x = std::clamp(mx / float(win_w), 0.0f, 1.0f);
        if (dragging_hsplit) ratio_y = std::clamp((my - menu_height) / float(content_h), 0.0f, 1.0f);
        if (dragging_rhsplit) right_ratio = std::clamp((my - menu_height) / float(content_h), 0.0f, 1.0f);
        drag_x = std::clamp(int(ratio_x * win_w), min_side, win_w - min_side);
        content_h = win_h - menu_height;
        drag_y = std::clamp(menu_height + int(ratio_y * content_h), menu_height + min_top, win_h - min_bot);
        right_split_y = std::clamp(menu_height + int(right_ratio * content_h), menu_height + min_top, menu_height + content_h - min_bot);
    }

    int top_h = std::max(1, drag_y - menu_height);
    Renderer::ImageData frameData = Renderer::RendererManager::rendererGL->CaptureFrame();
    if (frameData.width != drag_x || frameData.height != top_h) {
        Renderer::RendererManager::rendererGL->setSize(drag_x, top_h);
        frameData = Renderer::RendererManager::rendererGL->CaptureFrame();
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom((void*)frameData.pixels.data(), frameData.width, frameData.height, 32, frameData.width * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    struct nk_image nk_img = nk_image_ptr(texture);

    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderClear(renderer);

    EditorPanels::DrawTopMenu(win_w, menu_height);
    EditorPanels::DrawImageView(nk_img, drag_x, menu_height, top_h);
    EditorPanels::DrawAssetBrowser(drag_x, drag_y, win_h);

    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_Rect vr = { drag_x - 1, menu_height, 2, content_h };
    SDL_RenderFillRect(renderer, &vr);
    SDL_Rect hr = { 0, drag_y - 1, drag_x, 2 };
    SDL_RenderFillRect(renderer, &hr);

    int side_x = drag_x;
    int side_w = win_w - drag_x;
    int Hierarchy_h = right_split_y - menu_height;
    int prop_h = content_h - Hierarchy_h;

    EditorPanels::DrawHierarchy(side_x, side_w, menu_height, Hierarchy_h);
    EditorPanels::DrawProperties(side_x, Hierarchy_h, menu_height, side_w, prop_h);

    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_Rect rhr = { drag_x, right_split_y - 1, side_w, 2 };
    SDL_RenderFillRect(renderer, &rhr);

    nk_sdl_render(NK_ANTI_ALIASING_ON);
    SDL_RenderPresent(renderer);
    SDL_DestroyTexture(texture);
}

void Runtime::EditorRuntime::ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float dt) {
    Renderer::RendererManager::cam->ProcessInput(xpos, ypos, KeyIsDown, dt);
}

void Runtime::EditorRuntime::ProcessInput(GLFWwindow* window, float dt) {
    Renderer::RendererManager::cam->ProcessInput(window, dt);
}

void Runtime::EditorRuntime::Cleanup() {
    nk_sdl_shutdown();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
}
