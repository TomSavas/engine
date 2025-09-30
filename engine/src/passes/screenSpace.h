#pragma once

#include "engine.h"
#include "passes/blur.h"
#include "renderGraph.h"
#include "rhi/vulkan/bindless.h"

class VulkanBackend;

struct ScreenSpaceRenderer
{
    Pipeline ssrPipeline;

    BindlessTexture output;
};

[[nodiscard]]
auto ssrPass(std::optional<ScreenSpaceRenderer>& ssRenderer, std::optional<BlurRenderer>& blur, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> colorOutput, RenderGraphResource<BindlessTexture> normal,
    RenderGraphResource<BindlessTexture> positions, RenderGraphResource<BindlessTexture> reflectionUvs)
    -> RenderGraphResource<BindlessTexture>;