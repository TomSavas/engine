#include "passes/instancePicker.h"

#include "engine.h"

#include "GLFW/glfw3.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <vulkan/vulkan_core.h>

#include "debugUI.h"
#include "imgui.h"
#include "renderGraph.h"
#include "rhi/renderpass.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"
#include "scene.h"

#include <print>

auto instancePickerPass(std::optional<InstancePicker>& colorPicker, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> objectIds)
    -> void
{
    if (!colorPicker)
    {
        colorPicker = InstancePicker{};

        auto info = vkutil::init::bufferCreateInfo(sizeof(u32), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        for (size_t i{}; i < std::size(colorPicker->readbackBuffers); ++i)
        {
            const auto name = std::format("Instance picker readback #{}", i);
            colorPicker->readbackBuffers[i] = backend.allocateBuffer(name, info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }
    
    auto& pass = createPass(graph);
    pass.pass.debugName = "Instance picker readback";

    auto ids = readResource<BindlessTexture>(graph, pass, objectIds, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    pass.pass.draw = [&backend, &colorPicker, ids](const RenderContext& ctx, RenderPass& pass) -> void
    {
        ZoneScopedCpuGpuAuto("SDF scene pass", backend.currentFrame());

        const auto objectIdsTexture = backend.bindlessResources->getTexture(
            *getResource<BindlessTexture>(ctx.graph, ids));

        AllocatedBuffer& scheduledReadbackBuffer = colorPicker->readbackBuffers[ctx.frame.stats.frameIndex % std::size(colorPicker->readbackBuffers)];
        AllocatedBuffer& readbackBuffer = colorPicker->readbackBuffers[(ctx.frame.stats.frameIndex + 1) % std::size(colorPicker->readbackBuffers)];

        VkBufferImageCopy region {
            .bufferOffset = 0,
            .bufferRowLength = 1,
            .bufferImageHeight = 1,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = VkOffset3D{colorPicker->mousePos.x, colorPicker->mousePos.y, 0},
            .imageExtent = VkExtent3D{1, 1, 1},
        };
        // Schedule readback on buffer N
        vkCmdCopyImageToBuffer(ctx.cmd, objectIdsTexture.image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            scheduledReadbackBuffer.buffer, 1, &region);

        // Readback from buffer N+1
        u8* dataOnGpu;
        vmaMapMemory(backend.allocator, readbackBuffer.allocation, (void**)&dataOnGpu);
        memcpy(&colorPicker->lastHoveredInstance, dataOnGpu, sizeof(colorPicker->lastHoveredInstance));
        vmaUnmapMemory(backend.allocator, readbackBuffer.allocation);
    };
}
