#pragma once
#include "rhi/vulkan/renderpass.h"

class VulkanBackend;
class RenderGraph;

struct TestRenderer
{
    RenderPass::Pipeline pipeline;
};

void testPass(std::optional<TestRenderer>& renderer, VulkanBackend& backend, RenderGraph& graph);
