#include "buffer.h"

#include "renderGraph.h"
#include "rhi/vulkan/backend.h"

#include <print>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

template <>
auto addTransition<Buffer>(VulkanBackend& backend, CompiledRenderGraph::Node& node, Buffer* resource,
    Layout oldLayout, Layout newLayout)
    -> void
{
    VkBufferMemoryBarrier2 bufferBarrier = {};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferBarrier.pNext = nullptr;

    // TODO: we can improve this
    bufferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    bufferBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    bufferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    bufferBarrier.buffer = resource->buffer;

    bufferBarrier.offset = 0;
    bufferBarrier.size = resource->size;

    // std::println("transitioning buffer, size {}", resource->size);

    node.bufferBarriers.push_back(bufferBarrier);
}
