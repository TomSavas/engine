#pragma once

#include "renderGraph.h"

#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/buffer.h"

class VulkanBackend;

struct ShadowRenderer
{
    Pipeline pipeline;
    AllocatedBuffer cascadeParams;
    BindlessTexture shadowMap;
};

struct ShadowPassRenderGraphData
{
    RenderGraphResource<BindlessTexture> shadowMap;
    RenderGraphResource<Buffer> cascadeParams;
};

//auto simpleShadowPass(std::optional<ShadowRenderer>& shadowRenderer, VulkanBackend& backend,
//    RenderGraph& graph)
//    -> ShadowPassRenderGraphData;
[[nodiscard]]
auto csmPass(std::optional<ShadowRenderer>& shadowRenderer, VulkanBackend& backend,
    RenderGraph& graph, u8 cascadeCount = 4)
    -> ShadowPassRenderGraphData;
