#pragma once

#include "passes/blur.h"
#include "renderGraph.h"
#include "rhi/vulkan/bindless.h"

struct BloomRenderer
{
    Pipeline pipeline;

    BindlessTexture output;
    std::vector<BindlessTexture> intermediateTextures;
};

[[nodiscard]]
auto bloomPass(std::optional<BloomRenderer>& bloom, std::optional<BlurRenderer>& blur, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> input)
    -> RenderGraphResource<BindlessTexture>;
