// main.cpp
#include "Core/Application.h"
#include "Renderer/RendererManager.h"
#include <iostream>

int main() {
    // 1) ask the user
    std::cout << "Select renderer:\n"
              << "  1) DirectX 9\n"
              << "  2) OpenGL 2.1\n"
              << "Enter choice (1 or 2): ";
    int choice = 0;
    std::cin >> choice;

    using namespace Renderer;
    if (choice == 1) {
        RendererManager::SelectRenderer(RendererType::DirectX9);
    } else if (choice == 2) {
        RendererManager::SelectRenderer(RendererType::OpenGL21);
    } else {
        std::cerr << "Invalid choice, defaulting to DirectX 9\n";
        RendererManager::SelectRenderer(RendererType::DirectX9);
    }

    // 2) now run the app normally
    Core::Application app;
    app.Run();

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();  // Wait for user input
    return 0;
}
