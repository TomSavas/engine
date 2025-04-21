#pragma once

#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

struct AllocatedBuffer
{
    // TODO(savas): store size, stride, etc.
    VkBuffer buffer;
    VmaAllocation allocation;
};
