#pragma once

#include "vulkan/pipelineBuilder.h"

#include <functional>
#include <optional>
#include <string>

struct CompiledRenderGraph;
struct Scene;
struct RenderContext;

struct RenderPass
{
    std::string debugName;

    std::optional<Pipeline> pipeline;

    std::optional<std::function<void(const RenderContext&)>> prepare = std::nullopt;
    std::optional<std::function<void(const RenderContext&)>> beginRendering = std::nullopt;
    std::function<void(const RenderContext&, RenderPass&)> draw;
};
