#pragma once

#include "renderGraph.h"
#include "rhi/vulkan/utils/buffer.h"

#include <optional>

class VulkanBackend;
struct RenderGraph;

struct GeometryCulling
{
    AllocatedBuffer culledDraws;
};

struct CullingPassRenderGraphData
{
    RenderGraphResource<Buffer> culledDraws;
};

auto cpuFrustumCullingPass(std::optional<GeometryCulling>& geometryCulling, VulkanBackend& backend, RenderGraph& graph)
    -> CullingPassRenderGraphData;
