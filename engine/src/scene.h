#pragma once

#include "camera.h"
#include "mesh.h"

#include "rhi/vulkan/utils/texture.h"

#include "tiny_gltf.h"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <vector>
#include <string>

class GLFWwindow;

struct Scene 
{
    std::string name;

    Camera* activeCamera;
    Camera mainCamera;
    Camera debugCamera;

    std::vector<Mesh> meshes;
    std::vector<Vertex> vertexData;
    std::vector<uint32_t> indices;

    // TEMP: move to a texture pool
    std::vector<tinygltf::Image> images;

    bool worldPaused = true;

    Scene(std::string name) : name(name), activeCamera(&mainCamera) {}

    void update(float dt, float currentTimeMs, GLFWwindow* window);
    void addMeshes(tinygltf::Model& model);
};
