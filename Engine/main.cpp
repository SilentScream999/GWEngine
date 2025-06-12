#include "Core/Application.h"
#include <iostream>

int main() {
    Core::Application app;
    app.Run();
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();  // Wait for user input
    return 0;
}