#pragma once

#include "renderGraph.h"
#include "rhi/vulkan/utils/bindless.h"

struct BlurRenderer
{
    Pipeline dualKawaseDownPipeline;
    Pipeline dualKawaseUpPipeline;

    // TEMP:
    BindlessTexture output;
    std::vector<BindlessTexture> intermediateTextures;
};

[[nodiscard]]
auto dualKawaseBlur(std::optional<BlurRenderer>& blur, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<BindlessTexture> input, u8 downsampleCount = 2, f32 positionOffsetMultiplier = 1.f,
    f32 colorMultiplier = 1.f, bool useAdditiveBlending = false)
    -> RenderGraphResource<BindlessTexture>;
