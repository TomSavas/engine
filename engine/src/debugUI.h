#pragma once

#include <string>

class VulkanBackend;
struct Scene;

constexpr std::string GRAPHICS = "graphics";

auto drawDebugUI(VulkanBackend& backend, Scene& scene, double dt) -> void;
