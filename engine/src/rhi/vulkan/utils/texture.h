#pragma once

#include <cmath>
#include <optional>
#include <string>
#include <unordered_map>

#include "engine.h"
#include "rhi/vulkan/utils/image.h"

class VulkanBackend;

struct RawTexture
{
    void* data;
    u32 size;
    VkExtent3D extent;
    // u32 width;
    // u32 height;
};

struct MipOptions
{
    u8 startMip;
    u8 endMip;

    static auto generateAll(const RawTexture& rawTexture) -> MipOptions
    {
        return {
            .startMip = 0,
            .endMip = std::min(
                static_cast<u8>(std::log2(rawTexture.extent.width)),
                static_cast<u8>(std::log2(rawTexture.extent.height))
            ),
        };
    }
    static constexpr auto one() -> MipOptions
    {
        return {
            .startMip = 0,
            .endMip = 0
        };
    }

    auto count() const -> u8
    {
        return endMip - startMip + 1;
    }
};

// FIXME: remove this, and move mipCount to allocated Image
struct Texture
{
    std::optional<VkDescriptorSet> imguiDescriptorSet = std::nullopt;
    std::string name;

    bool loadedFromFile = false;

    VkSampler sampler = VK_NULL_HANDLE;
    AllocatedImage image;
    // VkImageView view;

    u32 mipCount;
    VkImageLayout layout;
    //
    // VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

Texture whiteTexture(VulkanBackend& backend, u32 dimension);
Texture blackTexture(VulkanBackend& backend, u32 dimension);
Texture errorTexture(VulkanBackend& backend, u32 dimension);

template <typename T>
struct CacheResult
{
    T data;
    bool fromCache;
};

struct Textures
{
    VulkanBackend* backend;
    std::unordered_map<std::string, Texture> textureCache;

    explicit Textures(VulkanBackend& backend) : backend(&backend) {}

    // TODO: more ergonomic mip options
    auto loadRaw(void* data, u32 size, u32 width, u32 height, bool generateMips, bool cache = false,
        std::string name = "") -> std::optional<std::tuple<Texture, std::string>>;
    // auto loadRaw(std::string name, RawTexture rawTexture, bool generateMips = false) -> CacheResult<Texture&>;
    auto unload(std::string name) -> void;
    auto unloadRaw(Texture texture) -> void;
};