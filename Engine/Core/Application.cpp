#include "Application.h"
#include "Logger.h"
#include "../Renderer/RendererManager.h"
#include <thread>
#include <chrono>

void Core::Application::Run() {
    Logger::Info("Engine starting...");

    if (!Renderer::RendererManager::InitRenderer()) {
        Logger::Error("No supported renderer could be initialized!");
        return;
    }

    Logger::Info("Entering main loop.");
    bool running = true;

    while (running) {
        Renderer::RendererManager::RenderFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));

        // Optional exit logic, for now it runs indefinitely
        // running = glfwWindowShouldClose(...) || PeekMessage(...) etc.
    }

    Renderer::RendererManager::Shutdown();
}
