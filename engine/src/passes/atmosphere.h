#pragma once
#include "renderGraph.h"
#include "rhi/vulkan/utils/bindless.h"

class VulkanBackend;

struct AtmosphereRenderer
{
    RenderPass::Pipeline pipeline;
};

void atmospherePass(std::optional<AtmosphereRenderer>& renderer, VulkanBackend& backend, RenderGraph& graph, RenderGraphResource<BindlessTexture> depthMap);
