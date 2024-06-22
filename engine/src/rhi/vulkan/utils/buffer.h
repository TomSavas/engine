#pragma once

#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

struct AllocatedBuffer
{
    // TODO(savas): store size, strice, etc.
    VkBuffer buffer;
    VmaAllocation allocation;
};
