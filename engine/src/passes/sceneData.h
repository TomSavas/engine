#pragma once

#include "renderGraph.h"
#include "rhi/vulkan/utils/buffer.h"

#include <optional>

struct VulkanBackend;
struct RenderGraph;

struct SceneDataUploader
{
    AllocatedBuffer allDraws;
    AllocatedBuffer lightList;
    AllocatedBuffer perModelBuffer;
};

struct SceneUploadRenderGraphData
{
    RenderGraphResource<Buffer> allDraws;
    RenderGraphResource<Buffer> lightList;
    RenderGraphResource<Buffer> perModelData;
};

auto sceneUploadPass(std::optional<SceneDataUploader>& sceneUploader, VulkanBackend& backend, RenderGraph& graph, Scene& scene)
    -> SceneUploadRenderGraphData;
