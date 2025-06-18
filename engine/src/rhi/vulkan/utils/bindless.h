#pragma once

#include "rhi/vulkan/utils/texture.h"

#include <vulkan/vulkan.h>

#include <stdint.h>
#include <unordered_set>
#include <vector>

#include "rhi/vulkan/descriptors.h"

using BindlessTexture = uint32_t;

struct VulkanBackend;

struct BindlessResources
{
	static constexpr BindlessTexture k_white = 0;
	static constexpr BindlessTexture k_black = 1;
	static constexpr BindlessTexture k_error = 2;

	VulkanBackend* backend;

	DescriptorAllocator bindlessDescPoolAllocator;

    VkDescriptorSet bindlessTexDesc;
    VkDescriptorSetLayout bindlessTexDescLayout;

    // CPU mirror of what data is in the GPU buffer
    std::vector<Texture> textures;
    BindlessTexture lastUsedIndex;
    int capacity;
    std::unordered_set<BindlessTexture> freeIndices; // All free indices that occur before lastUsedIndex
	
	explicit BindlessResources(VulkanBackend& backend);

	BindlessTexture addTexture(Texture texture);
	Texture getTexture(BindlessTexture handle, BindlessTexture defaultTexture = k_error);
	void removeTexture(BindlessTexture handle);
};
