#include "Mesh.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>   // for std::replace

Mesh::Mesh(const std::vector<Vertex>& verts,
           const std::vector<uint32_t>& inds)
  : vertices(verts), indices(inds)
{}

bool Mesh::LoadFromOBJ(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << path << "\n";
        return false;
    }

    std::vector<glm::vec3> tempPositions;
    std::vector<glm::vec2> tempTexCoords;
    std::vector<glm::vec3> tempNormals;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        if (type == "v") {
            glm::vec3 pos; iss >> pos.x >> pos.y >> pos.z;
            tempPositions.push_back(pos);
        } 
        else if (type == "vt") {
            glm::vec2 uv; iss >> uv.x >> uv.y;
            tempTexCoords.push_back(uv);
        } 
        else if (type == "vn") {
            glm::vec3 n; iss >> n.x >> n.y >> n.z;
            tempNormals.push_back(n);
        } 
        else if (type == "f") {
            for (int i = 0; i < 3; ++i) {
                std::string token; iss >> token;
                std::replace(token.begin(), token.end(), '/', ' ');
                std::istringstream tok(token);
                int vi, ti, ni;
                tok >> vi >> ti >> ni;

                Vertex vert{};
                vert.position = tempPositions[vi - 1];
                vert.texcoord = tempTexCoords[ti - 1];
                vert.normal   = tempNormals[ni - 1];

                vertices.push_back(vert);
                indices.push_back(static_cast<uint32_t>(indices.size()));
            }
        }
    }

    std::cout << "Loaded OBJ: " << path 
              << " (" << vertices.size() << " verts)\n";
    return true;
}
