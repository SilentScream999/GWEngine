#pragma once
#include <iostream>
#include <string>

namespace Logger {
    inline void Info(const std::string& msg)  { std::cout << "[INFO] " << msg << "\n"; }
    inline void Warn(const std::string& msg)  { std::cout << "[WARN] " << msg << "\n"; }
    inline void Error(const std::string& msg) { std::cerr << "[ERROR] " << msg << "\n"; }
}