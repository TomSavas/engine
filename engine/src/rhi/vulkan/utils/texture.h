#pragma once

#include "rhi/vulkan/utils/image.h"

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

struct Texture
{
	AllocatedImage image;
	VkImageView view;

	uint32_t mipCount;
};
