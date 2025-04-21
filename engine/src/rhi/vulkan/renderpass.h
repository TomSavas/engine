#pragma once 

#include <vulkan/vulkan_core.h>

#include <string>
#include <vector>
#include <functional>

struct RenderPass {
    std::string debugName;

    VkPipelineBindPoint pipelineBindPoint;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    std::function<void(VkCommandBuffer cmd, RenderPass&)> draw;
};

struct RenderGraph {
    std::vector<RenderPass> renderpasses;
};
