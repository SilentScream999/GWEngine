#pragma once

#include "Core/Transform.h"
#include <vector>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texcoord;
};

class Mesh {
public:
	// two-arg constructor so emplace_back in Model.cpp works
	Mesh() = default;
	Mesh(const std::vector<Vertex>& verts,
		 const std::vector<uint32_t>& inds);

	std::vector<Vertex>      vertices;
	std::vector<uint32_t>    indices;
	Transform                transform;
	bool                     is_castable = true;

	bool LoadFromOBJ(const std::string& path);
};
