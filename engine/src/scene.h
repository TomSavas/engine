#pragma once

#include "camera.h"
#include "mesh.h"

#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/texture.h"

#include "tiny_gltf.h"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <vector>
#include <string>

class GLFWwindow;
struct VulkanBackend;

struct Scene 
{
    std::string name;

    Camera* activeCamera;
    Camera mainCamera;
    Camera debugCamera;

    VulkanBackend& backend;

    std::vector<Mesh> meshes;
    std::vector<Vertex> vertexData;
    std::vector<uint32_t> indices;

    // TEMP: move to a texture pool
    std::vector<tinygltf::Image> images;
    glm::vec3 lightDir = glm::vec3(0.1, -1.0, 0.1);

    std::vector<BindlessTexture> bindlessImages;
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    AllocatedBuffer perModelBuffer;
    AllocatedBuffer indirectCommands;

    bool worldPaused = true;

    Scene(std::string name, VulkanBackend& backend) : name(name), activeCamera(&mainCamera), backend(backend) {}

    void update(float dt, float currentTimeMs, GLFWwindow* window);
    void load(const char* path);
    void addMeshes(tinygltf::Model& model, glm::vec3 offset = glm::vec3(0.f));
    void createBuffers();
};
