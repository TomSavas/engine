#pragma once

#include "renderGraph.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/buffer.h"

struct VulkanBackend;

struct ShadowRenderer
{
    RenderPass::Pipeline pipeline;
    AllocatedBuffer shadowMapData;
    BindlessTexture shadowMap;
};

struct ShadowPassRenderGraphData
{
    RenderGraphResource<BindlessTexture> shadowMap;
    RenderGraphResource<Buffer> cascadeData;
};

ShadowPassRenderGraphData simpleShadowPass(std::optional<ShadowRenderer>& shadowRenderer, VulkanBackend& backend,
    RenderGraph& graph);
ShadowPassRenderGraphData csmPass(std::optional<ShadowRenderer>& shadowRenderer, VulkanBackend& backend,
    RenderGraph& graph, int cascadeCount = 4);
