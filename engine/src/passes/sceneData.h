#pragma once

#include <optional>

struct VulkanBackend;
struct RenderGraph;

struct SceneDataUploader
{

};

auto sceneUploadPass(std::optional<SceneDataUploader>& sceneUploader, VulkanBackend& backend, RenderGraph& graph)
    -> void;
