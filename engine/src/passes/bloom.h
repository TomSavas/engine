#pragma once

#include "renderGraph.h"
#include "rhi/vulkan/utils/bindless.h"
#include "passes/blur.h"

struct BloomRenderer
{
    Pipeline pipeline;

    // TEMP:
    BindlessTexture output;
    std::vector<BindlessTexture> intermediateTextures;
};

[[nodiscard]]
auto bloomPass(std::optional<BloomRenderer>& bloom, std::optional<BlurRenderer>& blur, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> input)
    -> RenderGraphResource<BindlessTexture>;
