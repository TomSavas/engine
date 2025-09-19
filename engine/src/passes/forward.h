#pragma once

#include "renderGraph.h"

#include "passes/lightCulling.h"

class VulkanBackend;
struct RenderGraph;

struct ForwardOpaqueRenderer
{
    Pipeline pipeline;

    BindlessTexture color;
    BindlessTexture normal;
    BindlessTexture positions;
    BindlessTexture reflections;
};

struct ForwardRenderGraphData
{
    RenderGraphResource<BindlessTexture> color;
    RenderGraphResource<BindlessTexture> normal;
    RenderGraphResource<BindlessTexture> positions;
    RenderGraphResource<BindlessTexture> reflections;
};

[[nodiscard]]
auto opaqueForwardPass(std::optional<ForwardOpaqueRenderer>& forwardOpaqueRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<Buffer> culledDraws, RenderGraphResource<BindlessTexture> depthMap,
    RenderGraphResource<Buffer> shadowData, RenderGraphResource<BindlessTexture> shadowMap,
    LightData lightData)
    -> ForwardRenderGraphData;
