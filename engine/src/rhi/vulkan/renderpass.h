#pragma once

#include <vulkan/vulkan_core.h>

#include <functional>
#include <optional>
#include <string>

class CompiledRenderGraph;
class Scene;

struct RenderPass
{
    std::string debugName;

    struct Pipeline
    {
        VkPipelineBindPoint pipelineBindPoint;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
    };
    std::optional<Pipeline> pipeline;
    // VkRenderingInfo renderingInfo;

    std::optional<std::function<void(VkCommandBuffer cmd, CompiledRenderGraph&)>> beginRendering = std::nullopt;
    std::function<void(VkCommandBuffer cmd, CompiledRenderGraph&, RenderPass&, Scene&)> draw;

    // virtual void draw(VkCommandBuffer cmd);
    // virtual VkRenderingInfo renderingInfo();
};

// struct RenderGraph {
//     std::vector<RenderPass> renderpasses;
// };
