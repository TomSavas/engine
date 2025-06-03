#pragma once

#include "rhi/vulkan/utils/buffer.h"

struct GeometryCulling
{
    AllocatedBuffer culledDraws;
};

struct CullingPassRenderGraphData
{
    RenderGraphResource<Buffer> culledDraws;
};

CullingPassRenderGraphData cpuFrustumCullingPass(std::optional<GeometryCulling>& geometryCulling, RHIBackend& backend, RenderGraph& graph);
