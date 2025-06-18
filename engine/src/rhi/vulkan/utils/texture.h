#pragma once

#include "rhi/vulkan/utils/image.h"

#include <vulkan/vulkan.h>

#include <optional>
#include <string>
#include <unordered_map>

struct VulkanBackend;

// FIXME: remove this, and move mipCount to allocated Image
struct Texture
{
	AllocatedImage image;
	VkImageView view;

	uint32_t mipCount;
};

Texture whiteTexture(VulkanBackend& backend, uint32_t dimension);
Texture blackTexture(VulkanBackend& backend, uint32_t dimension);
Texture errorTexture(VulkanBackend& backend, uint32_t dimension);

struct Textures
{
    VulkanBackend* backend;
    std::unordered_map<std::string, Texture> textureCache;

    explicit Textures(VulkanBackend& backend) : backend(&backend) {}

    // TODO: more ergonomic mip options
    // std::optional<Texture> load(std::string path, bool generateMips = true, bool cache = false);
    std::optional<std::tuple<Texture, std::string>> loadRaw(void* data, int size, int width, int height, bool generateMips, bool cache = false, std::string name = "");
    void unload(std::string name);
    void unloadRaw(Texture texture);
};