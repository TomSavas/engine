#pragma once

#include "engine.h"

#include <functional>
#include <string>

class VulkanBackend;
struct Scene;

constexpr std::string GRAPHICS = "Graphics";
constexpr const char* GRAPHICS_CSTR = GRAPHICS.c_str();

constexpr std::string SCENE = "Scene";
constexpr const char* SCENE_CSTR = SCENE.c_str();

constexpr std::string INSPECTOR = "Inspector";
constexpr const char* INSPECTOR_CSTR = INSPECTOR.c_str();

constexpr std::string GRAPHICS_PASSES = "Render passes";
constexpr const char* GRAPHICS_PASSES_CSTR = GRAPHICS_PASSES.c_str();

struct DebugUI
{
    std::unordered_map<std::string, std::vector<std::function<void()>>> fns;
    std::string selectedNode;
};

extern DebugUI debugUI;

auto addDebugUI(DebugUI& debugUI, std::string parentId, std::function<void()> fn) -> void;

auto drawDebugUI(DebugUI& debugUI, VulkanBackend& backend, Scene& scene, f64 dt) -> void;
