#pragma once

#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/utils/buffer.h"

#include <optional>

struct VulkanBackend;
struct RenderGraph;
struct Scene;

RenderPass emptyPass(VulkanBackend& backend);

std::optional<RenderPass> infGrid(VulkanBackend& backend);
std::optional<RenderPass> zPrePass(VulkanBackend& backend, Scene& scene);
// TODO: change into a rendergraph resource
AllocatedBuffer cullingPass(VulkanBackend& backend, RenderGraph& graph, Scene& scene);
std::optional<RenderPass> shadowPass(VulkanBackend& backend, Scene& scene);
void basePass(VulkanBackend& backend, RenderGraph& graph, Scene& scene, AllocatedBuffer culledDraws);
