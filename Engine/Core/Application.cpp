#include "Application.h"
#include "Logger.h"
#include "../Renderer/RendererManager.h"
#include <thread>
#include <chrono>
#include "Runtime.h"
#include "Play.h"
#include "Editor.h"

void Core::Application::Run() {
	Logger::Info("Engine starting...");
	
	// chose the editor/play mode
	std::cout << "Select Mode:\n"
			  << "  1) Editor\n"
			  << "  2) Play\n"
			  << "Enter choice (1 or 2): ";
	int choice = 0;
	std::cin >> choice;
	Runtime::Runtime *runtime = nullptr;
	
	if (choice == 1) {
		runtime = new Runtime::EditorRuntime();
	} else if (choice == 2) {
		runtime = new Runtime::PlayRuntime();
	} else {
		std::cerr << "Invalid choice, defaulting to Play\n";
		runtime = new Runtime::PlayRuntime();
	}
	
	if (!Renderer::RendererManager::InitRenderer(runtime)) {
		Logger::Error("No supported renderer could be initialized!");
		return;
	}
	
	runtime->Init(); // this must be after the renderer so that it can send over the meshes and whatnot to the renderer after it is initialized.

	Logger::Info("Entering main loop.");
	bool running = true;

	while (running) {
		runtime->PrepareForFrameRender();
		Renderer::RendererManager::RenderFrame();
		std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 16 millis so about 62.5 fps ish probably

		// Optional exit logic, for now it runs indefinitely
		// running = glfwWindowShouldClose(...) || PeekMessage(...) etc.
	}

	Renderer::RendererManager::Shutdown();
}
