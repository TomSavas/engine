#pragma once

#include "engine.h"
#include "renderGraph.h"
#include "rhi/vulkan/utils/bindless.h"

class VulkanBackend;

struct ScreenSpaceRenderer
{
    Pipeline ssrPipeline;
};

[[nodiscard]]
auto ssrPass(std::optional<ScreenSpaceRenderer>& ssRenderer, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<BindlessTexture> colorOutput, RenderGraphResource<BindlessTexture> normal,
    RenderGraphResource<BindlessTexture> reflectionUvs)
    -> RenderGraphResource<BindlessTexture>;