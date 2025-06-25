#pragma once
#include "renderGraph.h"
#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/buffer.h"

struct ZPrePassRenderer
{
    RenderPass::Pipeline pipeline;
    BindlessTexture depthMap;
    AllocatedImage depth;
};

struct ZPrePassRenderGraphData
{
    RenderGraphResource<BindlessTexture> depthMap;
};

ZPrePassRenderGraphData zPrePass(std::optional<ZPrePassRenderer>& renderer, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<Buffer> culledDraws);
