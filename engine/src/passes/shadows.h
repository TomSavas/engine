#pragma once 

#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/bindless.h"
#include "render_graph.h"

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

ShadowPassRenderGraphData csmPass(std::optional<ShadowRenderer>& shadowRenderer, VulkanBackend& backend, RenderGraph& graph, int cascadeCount=4);
