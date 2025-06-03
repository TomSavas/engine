#pragma once

#include "rhi/vulkan/utils/image.h"

#include "vk_mem_alloc.h"

#include <optional>
#include <string>

#include <vulkan/vulkan.h>

struct Texture
{
	AllocatedImage image;
	VkImageView view;

	uint32_t mipCount;
};

struct Textures
{
    RHIBackend& backend;
    std::unordered_map<std::string, Texture> textureCache;

    Textures(RHIBackend& backend) : backend(backend) {}

    // TODO: more ergonomic mip options
    std::optional<Texture> load(std::string path, bool generateMips = true, bool cache = false);
    std::optional<Texture> loadRaw(void* data, int size, int width, int height, bool generateMips, bool cache = false, std::string name = "");
    void unload(std::string name);
    void unloadRaw(Texture texture);
};
