#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "engine.h"

struct AllocatedBuffer
{
    // TODO(savas): store size, stride, etc.
    VkBuffer buffer;
    VmaAllocation allocation;
    u32 size;
};

using Buffer = AllocatedBuffer;
