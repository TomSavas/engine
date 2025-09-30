#pragma once

#include "renderGraph.h"
#include "rhi/vulkan/bindless.h"
#include "rhi/vulkan/utils/buffer.h"

struct ZPrePassRenderer
{
    Pipeline pipeline;
    BindlessTexture depthMap;
};

struct ZPrePassRenderGraphData
{
    RenderGraphResource<BindlessTexture> depthMap;
};

[[nodiscard]]
auto zPrePass(std::optional<ZPrePassRenderer>& renderer, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<Buffer> culledDraws)
    -> ZPrePassRenderGraphData;
