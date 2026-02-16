#pragma once

#include "engine.h"
#include "renderGraph.h"
#include "rhi/vulkan/bindless.h"

class VulkanBackend;

struct ImguiRenderer
{  
    BindlessTexture composite;
};

[[nodiscard]]
auto imguiPass(std::optional<ImguiRenderer>& imguiRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> finalOutput)
    -> RenderGraphResource<BindlessTexture>;
