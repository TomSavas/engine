#pragma once

#include "engine.h"
#include "renderGraph.h"
#include "rhi/vulkan/bindless.h"

class VulkanBackend;

auto imguiPass(VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<BindlessTexture> output)
    -> void;
