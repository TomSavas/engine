#pragma once
#include "rhi/renderpass.h"

class VulkanBackend;
class RenderGraph;

struct TestRenderer
{
    Pipeline pipeline;
};

auto testPass(std::optional<TestRenderer>& renderer, VulkanBackend& backend, RenderGraph& graph) -> void;
