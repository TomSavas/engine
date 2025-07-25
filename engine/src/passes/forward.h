#pragma once
#include "lightCulling.h"
#include "renderGraph.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/buffer.h"

struct VulkanBackend;
class RenderGraph;

struct ForwardOpaqueRenderer
{
    RenderPass::Pipeline pipeline;
};

void opaqueForwardPass(std::optional<ForwardOpaqueRenderer>& forwardOpaqueRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<Buffer> culledDraws, RenderGraphResource<BindlessTexture> depthMap,
    RenderGraphResource<Buffer> shadowData, RenderGraphResource<BindlessTexture> shadowMap,
    LightData lightData);
