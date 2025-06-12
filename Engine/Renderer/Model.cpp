#include "Model.h"
#include "../Core/Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Renderer {
bool Model::LoadFromFile(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenNormals |
        aiProcess_OptimizeMeshes | aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices);

    if (!scene || !scene->HasMeshes()) {
        Logger::Error("Failed to load model: " + path);
        return false;
    }

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* meshSrc = scene->mMeshes[m];
        std::vector<Vertex> verts;
        std::vector<uint32_t> idxs;

        // Fill verts & idxs just like in Mesh::LoadFromOBJ...
        for (unsigned int i = 0; i < meshSrc->mNumVertices; ++i) {
            // convert aiMesh vertex → Vertex
        }
        for (unsigned int f = 0; f < meshSrc->mNumFaces; ++f) {
            // convert aiFace indices → idxs
        }

        // Now emplace into meshes vector
        meshes.emplace_back(verts, idxs);
    }
    Logger::Info("Model loaded: " + path);
    return true;
}
}
