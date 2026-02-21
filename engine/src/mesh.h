#pragma once

#include "engine.h"
#include "rhi/vulkan/backend.h"

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <span>
#include <optional>
#include <type_traits>

struct Vertex
{
    union
    {
        struct
        {
            f32 pos[4];
            f32 uv[4];
            f32 normal[4];
            f32 tangent[4];
        };
        f32 raw[4 * 4];
    };
};

struct Instance
{
    glm::mat4 modelTransform;
    glm::vec3 aabbMin;
    glm::vec3 aabbMax;
    glm::vec4 metallicRoughnessFactors;
    bool selected;
};

struct DefaultMaterial
{
    enum class Features : u64
    {
        NONE = 0,
        LIT = 1 << 0,
        WIREFRAME = 1 << 1,
        NORMAL_MAPPING = 1 << 2,
        PARALLAX = 1 << 3,
        HIGHLIGHT = 1 << 4,
        MAINTAIN_UV_DENSITY = 1 << 5,
        ALL = ~(u64)0,

        DEFAULT = LIT | NORMAL_MAPPING | PARALLAX,
    };

    union
    {
        struct
        {
            u32 albedo;
            u32 normalTexture;
            u32 metallicRoughnessTexture;
            u32 bumpTexture;
            f32 baseColor[4];
            f32 uvScaleOffset[4];
            Features features;
            u64 padding;
            // f32 uvScaleOffset[4];
        };
        f32 raw[16];
        // f32 raw[12];
    };
};

inline constexpr DefaultMaterial::Features operator|(DefaultMaterial::Features Lhs, DefaultMaterial::Features Rhs) {
    return static_cast<DefaultMaterial::Features>(
        static_cast<std::underlying_type_t<DefaultMaterial::Features>>(Lhs) |
        static_cast<std::underlying_type_t<DefaultMaterial::Features>>(Rhs));
}

inline constexpr DefaultMaterial::Features operator&(DefaultMaterial::Features Lhs, DefaultMaterial::Features Rhs) {
    return static_cast<DefaultMaterial::Features>(
        static_cast<std::underlying_type_t<DefaultMaterial::Features>>(Lhs) &
        static_cast<std::underlying_type_t<DefaultMaterial::Features>>(Rhs));
}

inline constexpr DefaultMaterial::Features operator^(DefaultMaterial::Features Lhs, DefaultMaterial::Features Rhs) {
    return static_cast<DefaultMaterial::Features>(
        static_cast<std::underlying_type_t<DefaultMaterial::Features>>(Lhs) ^
        static_cast<std::underlying_type_t<DefaultMaterial::Features>>(Rhs));
}


// struct Instance
// {
//     std::string debugName;  
//     u16 material;

//     glm::mat4 modelTransform;
//     glm::vec3 aabbMin;
//     glm::vec3 aabbMax;
//     glm::vec4 metallicRoughnessFactors;
//     bool selected;
// }

struct Mesh
{
    std::string debugName;

    i32 vertexOffset;
    i32 vertexCount;

    i32 indexOffset;
    i32 indexCount;

    glm::vec3 aabbMin;
    glm::vec3 aabbMax;

    std::vector<Instance> instances;

    // TODO: Move out this to a standalone material
    // i32 materialIndex;
    i16 albedoTexture;
    i16 metallicRoughnessTexture;
    i16 normalTexture;
    i16 bumpTexture;
};








using ModelHandle = u16;
using InstanceHandle = u16;

struct Models
{
    AllocatedBuffer vertexBuffer;
    u32 vertexBytesWritten;
    AllocatedBuffer indexBuffer;
    u32 indexBytesWritten;
    AllocatedBuffer instanceBuffer;

    struct ModelData
    {
        std::vector<Vertex> vertices;
        std::vector<u32> indices;
        u64 hash;
    };
    struct Model
    {
        u32 vertexOffset;
        u32 vertexByteOffset;
        u32 vertexCount;

        u32 indexOffset;
        u32 indexByteOffset;
        u32 indexCount;

        u64 hash;

        u32 firstInstance;
        u32 instances;
    };
    struct ModelDebug
    {
        std::string name;
    };
    std::vector<Model> models;
    std::vector<ModelDebug> modelDebug;

    struct InstanceData
    {
        glm::mat4 transform;
        glm::vec4 material;
        // TODO: add aabb and all that jazz
    };
    struct InstanceDebug
    {
        std::string name;
        bool selected;
    };
    std::unordered_map<ModelHandle, std::vector<InstanceData>> instances;
    std::unordered_map<ModelHandle, std::vector<InstanceDebug>> instanceDebug;

    u32 modelCount;
    u32 instanceCount;
};

auto initModels(VulkanBackend& backend, u32 vertexSizeHint, u32 indexSizeHint, u32 instanceCountHint) -> Models;
auto loadModels(VulkanBackend& backend, Models& models, std::span<Models::ModelData> data, std::optional<std::span<Models::ModelDebug>> debugData = std::nullopt) -> std::vector<ModelHandle>;
auto addInstances(VulkanBackend& backend, Models& models, ModelHandle handle, std::span<Models::InstanceData> data, std::optional<std::span<Models::InstanceDebug>> debugData = std::nullopt) -> std::vector<InstanceHandle>;










