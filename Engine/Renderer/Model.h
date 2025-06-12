#pragma once

#include "Mesh.h"
#include <vector>
#include <string>

namespace Renderer {
class Model {
public:
    std::vector<Mesh> meshes;
    bool LoadFromFile(const std::string& path);
};
}
