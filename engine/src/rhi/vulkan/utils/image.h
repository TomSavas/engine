#pragma once

#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

struct AllocatedImage
{
    VkImage       image;
    VkExtent3D    extent;
    VkFormat      format;
    VkImageView   view;

    VmaAllocation allocation;
};

namespace vkutil::image
{
void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
void blitImageToImage(VkCommandBuffer cmd, VkImage src, VkExtent2D srcSize, VkImage dst, VkExtent2D dstSize);
}
