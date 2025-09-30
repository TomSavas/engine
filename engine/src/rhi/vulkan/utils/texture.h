#pragma once

#include "engine.h"

#include "rhi/vulkan/utils/image.h"

#include <optional>
#include <string>
#include <unordered_map>

class VulkanBackend;

// FIXME: remove this, and move mipCount to allocated Image
struct Texture
{
    AllocatedImage image;
    VkImageView view;

    u32 mipCount;
};

Texture whiteTexture(VulkanBackend& backend, u32 dimension);
Texture blackTexture(VulkanBackend& backend, u32 dimension);
Texture errorTexture(VulkanBackend& backend, u32 dimension);

struct Textures
{
    VulkanBackend* backend;
    std::unordered_map<std::string, Texture> textureCache;

    explicit Textures(VulkanBackend& backend) : backend(&backend) {}

    // TODO: more ergonomic mip options
    auto loadRaw(void* data, u32 size, u32 width, u32 height, bool generateMips, bool cache = false,
        std::string name = "") -> std::optional<std::tuple<Texture, std::string>>;
    auto unload(std::string name) -> void;
    auto unloadRaw(Texture texture) -> void;
};