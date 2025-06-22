#pragma once

#include <optional>

#include "renderGraph.h"
#include "rhi/vulkan/utils/buffer.h"

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

CullingPassRenderGraphData cpuFrustumCullingPass(
    std::optional<GeometryCulling>& geometryCulling, VulkanBackend& backend, RenderGraph& graph);
