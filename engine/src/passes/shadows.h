#include "passes/passes.h"
#include "scene.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"

#include "imgui.h"

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

struct ShadowRenderer
{
    Pipeline pipeline;
    AllocatedBuffer shadowMapData;
    BindlessResources::Handle shadowMap;
};

struct ShadowPassRenderGraphData
{
    RenderGraphResource<BindlessTexture> shadowMap;
    RenderGraphResource<Buffer> data;
};

ShadowPassRenderGraphData csmPass(ShadowRenderer& shadowRenderer, RHIBackend& backend, RenderGraph& graph, int cascadeCount);
