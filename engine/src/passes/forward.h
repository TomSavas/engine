#pragma once

#include "renderGraph.h"

#include "passes/lightCulling.h"

class VulkanBackend;
struct RenderGraph;

struct ForwardOpaqueRenderer
{
    Pipeline pipeline;
};

auto opaqueForwardPass(std::optional<ForwardOpaqueRenderer>& forwardOpaqueRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<Buffer> culledDraws, RenderGraphResource<BindlessTexture> depthMap,
    RenderGraphResource<Buffer> shadowData, RenderGraphResource<BindlessTexture> shadowMap,
    LightData lightData)
    -> void;
