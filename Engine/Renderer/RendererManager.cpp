#include "RendererManager.h"
#include "RendererDX9.h"
#include "RendererGL21.h"
#include "../Core/Logger.h"
#include "Mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <memory>

namespace Renderer {
    // Static member definitions
    RendererType RendererManager::s_selectedRenderer = RendererType::DirectX9;
    IRenderer*   RendererManager::rendererDX9       = nullptr;
    IRenderer*   RendererManager::rendererGL        = nullptr;
    Camera*      RendererManager::cam               = new Camera();

    void RendererManager::SelectRenderer(RendererType type) {
        s_selectedRenderer = type;
    }

    bool RendererManager::InitRenderer() {
        // Initialize camera
        cam->position = glm::vec3(0.0f, 1.0f, 5.0f);
        cam->rotation = glm::vec3(0.0f);

        bool success = false;

        // Initialize only the selected renderer
        if (s_selectedRenderer == RendererType::DirectX9) {
            Logger::Info("Initializing DirectX 9 renderer...");
            rendererDX9 = new RendererDX9();
            if (rendererDX9->Init(cam)) {
                Logger::Info("DirectX 9 renderer initialized.");
                success = true;
            } else {
                Logger::Error("DirectX 9 renderer failed to initialize.");
                delete rendererDX9;
                rendererDX9 = nullptr;
            }
        } else if (s_selectedRenderer == RendererType::OpenGL21) {
            Logger::Info("Initializing OpenGL 2.1 renderer...");
            rendererGL = new RendererGL21();
            if (rendererGL->Init(cam)) {
                Logger::Info("OpenGL 2.1 renderer initialized.");
                success = true;
            } else {
                Logger::Error("OpenGL 2.1 renderer failed to initialize.");
                delete rendererGL;
                rendererGL = nullptr;
            }
        }

        if (!success) {
            Logger::Error("No renderer could be initialized.");
            return false;
        }

        // Load meshes
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

        // Assign meshes to the active renderer
        if (rendererDX9) {
            if (rendererDX9->SetMeshes(meshes)) {
                Logger::Info("Loaded meshes for DirectX 9");
            } else {
                Logger::Error("Could not load meshes for DirectX 9");
            }
        }
        if (rendererGL) {
            if (rendererGL->SetMeshes(meshes)) {
                Logger::Info("Loaded meshes for OpenGL 2.1");
            } else {
                Logger::Error("Could not load meshes for OpenGL 2.1");
            }
        }

        return true;
    }

    void RendererManager::RenderFrame() {
        cam->updateForFrame();
        if (rendererDX9) rendererDX9->RenderFrame();
        if (rendererGL)  rendererGL->RenderFrame();
    }

    void RendererManager::Shutdown() {
        delete rendererDX9;
        delete rendererGL;
        rendererDX9 = nullptr;
        rendererGL  = nullptr;
    }
}