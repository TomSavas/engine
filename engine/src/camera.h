#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

struct Camera
{
    float verticalFov = M_PI / 4;

    float moveSpeed = 1.f;
    float rotationSpeed = 0.002f;

    float nearClippingPlaneDist = 0.1f;
    float farClippingPlaneDist = 100000.f;

    float aspectRatio = 16.f / 9.f;

    glm::vec3 position = glm::vec3(0.f, 5.f, 10.f);
    // glm::mat4 rotation = glm::mat4(1.f);
    glm::quat rotation = glm::quat();
};
