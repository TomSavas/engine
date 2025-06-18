#pragma once

#include "render_graph.h"

#include "rhi/vulkan/utils/buffer.h"

#include <optional>

struct VulkanBackend;
class RenderGraph;

struct GeometryCulling
{
    AllocatedBuffer culledDraws;
};

struct CullingPassRenderGraphData
{
    RenderGraphResource<Buffer> culledDraws;
};

CullingPassRenderGraphData cpuFrustumCullingPass(std::optional<GeometryCulling>& geometryCulling, VulkanBackend& backend, RenderGraph& graph);
