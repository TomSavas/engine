#pragma once

#include "renderGraph.h"
#include "rhi/vulkan/bindless.h"

#include <optional>

class VulkanBackend;
struct RenderGraph;

struct SdfRenderer
{
    Pipeline pipeline;

    BindlessTexture color;
};

struct SdfRenderGraphData
{
    RenderGraphResource<BindlessTexture> color;
};

[[nodiscard]]
auto sdfGeometryPass(std::optional<SdfRenderer>& sdfRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> output)
    // -> SdfRenderGraphData;
    -> RenderGraphResource<BindlessTexture>;
