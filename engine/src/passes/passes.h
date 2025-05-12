#pragma once

#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/image.h"

#include <glm/glm.hpp>

#include <optional>

struct VulkanBackend;
struct RenderGraph;
struct Scene;

struct ShadowPassData
{
    glm::mat4 lightViewProjMatrices[4];
    glm::mat4 invLightViewProjMatrices[4];
    float cascadeDistances[4];
    int cascadeCount;
};

struct GPUShadowPassData 
{
    AllocatedImage shadowMap;  
    AllocatedBuffer shadowMapData;
};

RenderPass emptyPass(VulkanBackend& backend);

std::optional<RenderPass> infGrid(VulkanBackend& backend);
std::optional<RenderPass> zPrePass(VulkanBackend& backend, Scene& scene);
// TODO: change into a rendergraph resource
AllocatedBuffer cullingPass(VulkanBackend& backend, RenderGraph& graph, Scene& scene);
GPUShadowPassData* shadowPass(VulkanBackend& backend, RenderGraph& graph, Scene& scene, int cascadeCount = 4);
void basePass(VulkanBackend& backend, RenderGraph& graph, Scene& scene, AllocatedBuffer culledDraws, GPUShadowPassData* shadowPassData);
