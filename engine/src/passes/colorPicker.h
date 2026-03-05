#pragma once

#include "renderGraph.h"
#include "rhi/vulkan/bindless.h"
#include "rhi/vulkan/utils/buffer.h"

#include <glm/glm.hpp>

#include <optional>

class VulkanBackend;
struct RenderGraph;

struct ColorPicker
{
    AllocatedBuffer readbackBuffers[2];
    glm::dvec2 mousePos;
    u32 lastHoveredInstance;
};

auto colorPickerPass(std::optional<ColorPicker>& sdfRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> objectIds)
    -> void;
