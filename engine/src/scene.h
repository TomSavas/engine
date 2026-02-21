#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "camera.h"
#include "mesh.h"
#include "result.hpp"
#include "rhi/vulkan/bindless.h"
#include "rhi/vulkan/utils/buffer.h"
#include "sceneGraph.h"
#include "tiny_gltf.h"

// TODO: extract into materials
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/backend.h"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <numeric>

class GLFWwindow;
class VulkanBackend;

enum class assetError
{
};

struct PointLight
{
    // TODO: pack
    glm::vec4 pos;
    glm::vec4 color;
    glm::vec4 rangeAndStrength;
};

using MaterialHandle = u32;

template<>
struct std::hash<DefaultMaterial>
{
    std::size_t operator()(const DefaultMaterial& v) const noexcept
    {
        auto hash = static_cast<u64>(v.raw[0]);
        for (u32 i = 1; i < std::size(v.raw); i++)
        {
            hash ^= static_cast<u64>(v.raw[i]) + static_cast<u64>(0x9e3779b9) + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

template<typename T>
    requires Hashable<T>
struct Materials
{
    AllocatedBuffer buffer;

    std::vector<T> materials;
    std::unordered_map<size_t, size_t> hashToIndex;
};

template<typename T>
auto initMaterials(VulkanBackend& backend, u32 countHint) -> Materials<T>
{
    Materials<T> materials;
    materials.materials.reserve(countHint);

    const auto info = vkutil::init::bufferCreateInfo(countHint * sizeof(T),
                                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    materials.buffer = backend.allocateBuffer("Material buffer", info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    return materials;
}

template<typename T>
auto addMaterials(VulkanBackend& backend, Materials<T>& materials, std::span<T> materialData) -> std::vector<MaterialHandle>
{
    // TODO: realloc if needed
    const auto firstIndex = materials.materials.size();
    materials.materials.insert(materials.materials.end(), materialData.begin(), materialData.end());
    const auto size = materials.materials.size() * sizeof(T);
    backend.copyBufferWithStaging(std::nullopt, materials.materials.data(), size, materials.buffer.buffer);

    std::vector<MaterialHandle> handles(materialData.size());
    std::iota(handles.begin(), handles.end(), firstIndex);
    return handles;
}

struct Scene
{
    std::string name;

    Models models;
    Materials<DefaultMaterial> materials;

    Camera* activeCamera;
    Camera mainCamera;
    Camera debugCamera;

    // TEMP: this should live in some gameplay systems
    std::vector<PointLight> pointLights;

    VulkanBackend& backend;

    SceneGraph sceneGraph;

    glm::vec3 aabbMin = glm::vec3(0.f);
    glm::vec3 aabbMax = glm::vec3(0.f);

    //std::vector<Mesh> meshes;
    std::unordered_map<std::string, Mesh> meshes;
    std::vector<Vertex> vertexData;
    std::vector<u32> indices;

    // TEMP: move to a texture pool
    std::vector<tinygltf::Image> images;
    glm::vec3 lightDir = glm::vec3(0.6, -1.0, 0.175);

    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    AllocatedBuffer indirectCommands;

    // TEMP:
    u32 meshCount;

    bool worldPaused = true;

    Scene(std::string name, VulkanBackend& backend) : name(name), activeCamera(&mainCamera), backend(backend), meshCount(0)
    {
        sceneGraph.root = new SceneGraph::Node("root", glm::mat4(1.f), glm::mat4(1.f), 0, nullptr);
    }

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
        vertexBuffer = other.vertexBuffer;
        indexBuffer = other.indexBuffer;
        indirectCommands = other.indirectCommands;
        meshCount = other.meshCount;
        sceneGraph = other.sceneGraph;
        models = other.models;
        materials = other.materials;
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
        vertexBuffer = other.vertexBuffer;
        indexBuffer = other.indexBuffer;
        indirectCommands = other.indirectCommands;
        meshCount = other.meshCount;
        sceneGraph = other.sceneGraph;
        models = other.models;
        materials = other.materials;
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
        vertexBuffer = other.vertexBuffer;
        indexBuffer = other.indexBuffer;
        indirectCommands = other.indirectCommands;
        meshCount = other.meshCount;
        sceneGraph = other.sceneGraph;
        models = other.models;
        materials = other.materials;
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
        vertexBuffer = other.vertexBuffer;
        indexBuffer = other.indexBuffer;
        indirectCommands = other.indirectCommands;
        meshCount = other.meshCount;
        sceneGraph = other.sceneGraph;
        models = other.models;
        materials = other.materials;
        return *this;
    }

    void update(f32 dt, f32 currentTimeMs, GLFWwindow* window, bool shouldHandleInput);
    void load(const char* path);
    void addModel(tinygltf::Model& model, glm::mat4 transform = glm::mat4(1.f));
    void addNodes(tinygltf::Model& model, tinygltf::Node& node, glm::mat4 transform, SceneGraph::Node& parent);
    void addMesh(tinygltf::Model& model, tinygltf::Mesh& mesh, glm::mat4 transform, SceneGraph::Node& parent);
    void createBuffers();

    auto addMesh(tinygltf::Model& model, tinygltf::Mesh& mesh, std::vector<glm::mat4> transforms, std::vector<SceneGraph::NodeHandle>& nodes) -> void;
    auto addNodes(tinygltf::Model& model, tinygltf::Node& node, std::vector<glm::mat4> transforms, std::vector<SceneGraph::NodeHandle>& parentNodes) -> void;
    // TODO: should return a list of model and instance handles probably?
    auto addScene(const char* path, std::vector<glm::mat4> transforms) -> void;
};

result::result<Scene, assetError> loadScene(VulkanBackend& backend, std::string name, std::string path,
    u32 lightCount);
Scene emptyScene(VulkanBackend& backend);

struct ModelData
{
    glm::vec4 textures;
    glm::vec4 selected;
    glm::vec4 metallicRoughnessFactors;
    glm::mat4 model;
};

auto gatherModelData(Scene& scene) -> std::vector<ModelData>;
