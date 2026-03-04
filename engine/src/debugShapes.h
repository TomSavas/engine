#pragma once

#include "scene.h"

#include "glm/glm.hpp"

class VulkanBackend;
struct Scene;

auto cubeModelData() -> Models::ModelData;
auto sphereModelData(u8 horizontalSubdivisions = 16, u8 verticalSubdivisions = 16) -> Models::ModelData;
auto planeModelData() -> Models::ModelData;

auto debugDrawCube(Scene& scene, glm::vec3 center, glm::vec3 scale, glm::vec3 color) -> void;
auto debugDrawSphere(Scene& scene, glm::vec3 center, glm::vec3 scale, glm::vec3 color) -> void;
auto debugDrawPlane(Scene& scene, glm::vec3 center, glm::vec3 scale, glm::vec3 color) -> SceneGraph::NodeHandle;
