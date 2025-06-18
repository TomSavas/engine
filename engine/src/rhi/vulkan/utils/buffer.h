#pragma once

#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

using Buffer = VkBuffer;

struct AllocatedBuffer
{
    // TODO(savas): store size, stride, etc.
    VkBuffer buffer;
    VmaAllocation allocation;
};
