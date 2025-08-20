#pragma once

#include "engine.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

struct Camera
{
    f32 verticalFov = M_PI / 4;

    f32 moveSpeed = 700.f;
    f32 rotationSpeed = 0.002f;

    f32 nearClippingPlaneDist = 10.f;
    f32 farClippingPlaneDist = 5000.f;

    f32 aspectRatio = 16.f / 9.f;

    glm::vec3 position = glm::vec3(0.f, 2.5f, 15.f);
    glm::mat4 rotation = glm::mat4(1.f);

    [[nodiscard]]
    auto view() const -> glm::mat4
    {
        return glm::inverse(glm::translate(glm::mat4(1.f), position) * rotation);
    }

    [[nodiscard]]
    auto proj() const -> glm::mat4
    {
        glm::mat4 proj = glm::perspective<f32>(verticalFov, aspectRatio, nearClippingPlaneDist,
            farClippingPlaneDist);
        proj[1][1] *= -1.f;
        return proj;
    }
};
