#pragma once

#include "renderGraph.h"
#include "rhi/vulkan/bindless.h"

class VulkanBackend;

struct AtmosphereRenderer
{
    Pipeline pipeline;
};

auto atmospherePass(std::optional<AtmosphereRenderer>& atmosphere, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<BindlessTexture> depthMap, RenderGraphResource<BindlessTexture> input)
    -> RenderGraphResource<BindlessTexture>;
