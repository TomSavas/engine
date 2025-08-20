#pragma once

#include "vulkan/pipelineBuilder.h"

#include <vulkan/vulkan_core.h>

#include <functional>
#include <optional>
#include <string>

struct CompiledRenderGraph;
class Scene;

struct RenderPass
{
    std::string debugName;

    std::optional<Pipeline> pipeline;

    std::optional<std::function<void(VkCommandBuffer cmd, CompiledRenderGraph&)>> beginRendering = std::nullopt;
    std::function<void(VkCommandBuffer cmd, CompiledRenderGraph&, RenderPass&, Scene&)> draw;
};