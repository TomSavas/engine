#pragma once
#include "renderGraph.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/buffer.h"

struct LightCulling
{
    RenderPass::Pipeline pipeline;

    AllocatedBuffer lightList;
    AllocatedBuffer lightIndexList;
    AllocatedBuffer lightGrid;
};

struct LightData
{
    RenderGraphResource<Buffer> lightList;
    RenderGraphResource<Buffer> lightIndexList;
    RenderGraphResource<Buffer> lightGrid; // Might be 2d or 3d depending on culling algorithm
};

LightData tiledLightCullingPass(std::optional<LightCulling>& lightCulling, VulkanBackend& backend, RenderGraph& graph);
LightData clusteredLightCullingPass(std::optional<LightCulling>& lightCulling, VulkanBackend& backend,
    RenderGraph& graph);
