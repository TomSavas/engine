#pragma once

#include "renderGraph.h"
#include "rhi/vulkan/utils/bindless.h"

class VulkanBackend;

struct AtmosphereRenderer
{
    Pipeline pipeline;
};

auto atmospherePass(std::optional<AtmosphereRenderer>& renderer, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<BindlessTexture> depthMap)
    -> void;
