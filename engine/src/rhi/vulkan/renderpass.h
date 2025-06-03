#pragma once 

#include <vulkan/vulkan_core.h>

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <utility>

struct RenderPass {
    std::string debugName;

    struct Pipeline {
        VkPipelineBindPoint pipelineBindPoint;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
    };
    std::optional<Pipeline> pipeline;
    // VkRenderingInfo renderingInfo;

    std::function<void(VkCommandBuffer cmd, RenderPass&)> draw;
    std::function<VkRenderingInfo(RenderPass&)> renderingInfo;

    // virtual void draw(VkCommandBuffer cmd);
    // virtual VkRenderingInfo renderingInfo();
};

struct RenderGraph {
    std::vector<RenderPass> renderpasses;
};
