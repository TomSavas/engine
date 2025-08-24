#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>

#include "camera.h"
#include "mesh.h"
#include "result.hpp"
#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/texture.h"
#include "tiny_gltf.h"

class GLFWwindow;
struct VulkanBackend;

enum class assetError
{
};

struct PointLight
{
    // TODO: pack
    glm::vec4 pos;
    glm::vec4 color;
    glm::vec4 range;
};

struct Scene
{
    std::string name;

    Camera* activeCamera;
    Camera mainCamera;
    Camera debugCamera;

    // TEMP: this should live in some gameplay systems
    std::vector<PointLight> pointLights;

    VulkanBackend& backend;

    glm::vec3 aabbMin = glm::vec3(0.f);
    glm::vec3 aabbMax = glm::vec3(0.f);

    std::vector<Mesh> meshes;
    std::vector<Vertex> vertexData;
    std::vector<u32> indices;

    // TEMP: move to a texture pool
    std::vector<tinygltf::Image> images;
    glm::vec3 lightDir = glm::vec3(0.6, -1.0, 0.175);

    std::vector<BindlessTexture> bindlessImages;
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    AllocatedBuffer perModelBuffer;
    AllocatedBuffer indirectCommands;

    bool worldPaused = true;

    Scene(std::string name, VulkanBackend& backend) : name(name), activeCamera(&mainCamera), backend(backend) {}

    Scene(Scene& other) : Scene(other.name, other.backend)
    {
        name = other.name;
        mainCamera = other.mainCamera;
        debugCamera = other.debugCamera;
        activeCamera = &mainCamera;
        pointLights = other.pointLights;
        aabbMin = other.aabbMin;
        aabbMax = other.aabbMax;
        meshes = other.meshes;
        vertexData = other.vertexData;
        indices = other.indices;
        images = other.images;
        lightDir = other.lightDir;
        bindlessImages = other.bindlessImages;
        vertexBuffer = other.vertexBuffer;
        indexBuffer = other.indexBuffer;
        perModelBuffer = other.perModelBuffer;
        indirectCommands = other.indirectCommands;
    }

    Scene(Scene&& other) : Scene(other.name, other.backend)
    {
        name = other.name;
        mainCamera = other.mainCamera;
        debugCamera = other.debugCamera;
        activeCamera = &mainCamera;
        pointLights = other.pointLights;
        aabbMin = other.aabbMin;
        aabbMax = other.aabbMax;
        meshes = other.meshes;
        vertexData = other.vertexData;
        indices = other.indices;
        images = other.images;
        lightDir = other.lightDir;
        bindlessImages = other.bindlessImages;
        vertexBuffer = other.vertexBuffer;
        indexBuffer = other.indexBuffer;
        perModelBuffer = other.perModelBuffer;
        indirectCommands = other.indirectCommands;
    }

    Scene& operator=(Scene& other)
    {
        name = other.name;
        mainCamera = other.mainCamera;
        debugCamera = other.debugCamera;
        activeCamera = &mainCamera;
        meshes = other.meshes;
        pointLights = other.pointLights;
        aabbMin = other.aabbMin;
        aabbMax = other.aabbMax;
        vertexData = other.vertexData;
        indices = other.indices;
        images = other.images;
        lightDir = other.lightDir;
        bindlessImages = other.bindlessImages;
        vertexBuffer = other.vertexBuffer;
        indexBuffer = other.indexBuffer;
        perModelBuffer = other.perModelBuffer;
        indirectCommands = other.indirectCommands;
        return *this;
    }

    Scene& operator=(Scene&& other)
    {
        name = other.name;
        mainCamera = other.mainCamera;
        debugCamera = other.debugCamera;
        activeCamera = &mainCamera;
        meshes = other.meshes;
        pointLights = other.pointLights;
        aabbMin = other.aabbMin;
        aabbMax = other.aabbMax;
        vertexData = other.vertexData;
        indices = other.indices;
        images = other.images;
        lightDir = other.lightDir;
        bindlessImages = other.bindlessImages;
        vertexBuffer = other.vertexBuffer;
        indexBuffer = other.indexBuffer;
        perModelBuffer = other.perModelBuffer;
        indirectCommands = other.indirectCommands;
        return *this;
    }

    void update(f32 dt, f32 currentTimeMs, GLFWwindow* window);
    void load(const char* path);
    void addMeshes(tinygltf::Model& model, glm::vec3 offset = glm::vec3(0.f));
    void createBuffers();
};

result::result<Scene, assetError> loadScene(VulkanBackend& backend, std::string name, std::string path,
    u32 lightCount);
Scene emptyScene(VulkanBackend& backend);
