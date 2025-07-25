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
    // TEMP:
    AllocatedBuffer lightCount;
};

struct LightData
{
    RenderGraphResource<BindlessTexture> depthMap;
    RenderGraphResource<Buffer> lightList;
    RenderGraphResource<Buffer> lightIndexList;
    RenderGraphResource<Buffer> lightGrid; // Might be 2d or 3d depending on culling algorithm
    // TEMP:
    RenderGraphResource<Buffer> lightCount;
};

LightData tiledLightCullingPass(std::optional<LightCulling>& lightCulling, VulkanBackend& backend, RenderGraph& graph,
    Scene& scene, RenderGraphResource<BindlessTexture> depthMap, float tileSizeAsPercentageOfScreen);
LightData clusteredLightCullingPass(std::optional<LightCulling>& lightCulling, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> depthMap, Scene& scene);
