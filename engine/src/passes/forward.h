#pragma once
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
    RenderGraph& graph, RenderGraphResource<Buffer> culledDraws, RenderGraphResource<Buffer> shadowData,
    RenderGraphResource<BindlessTexture> shadowMap);
