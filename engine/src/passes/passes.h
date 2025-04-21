#pragma once

#include "rhi/vulkan/renderpass.h"

#include <optional>

struct VulkanBackend;
struct Scene;
RenderPass emptyPass(VulkanBackend& backend);
std::optional<RenderPass> infGrid(VulkanBackend& backend);
std::optional<RenderPass> basePass(VulkanBackend& backend, Scene& scene);

