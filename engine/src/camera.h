#pragma once

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>

struct Camera
{
    float verticalFov = M_PI / 4;

    float moveSpeed = 1.f;
    float rotationSpeed = 0.002f;

    float nearClippingPlaneDist = 0.1f;
    float farClippingPlaneDist = 100000.f;

    float aspectRatio = 16.f / 9.f;

    glm::vec3 position = glm::vec3(0.f, 2.5f, 15.f);
    glm::mat4 rotation = glm::mat4(1.f);

    glm::mat4 view()
    {
        return glm::inverse(glm::translate(glm::mat4(1.f), position) * rotation);
    }
    glm::mat4 proj()
    {
        // glm::mat4 proj = glm::perspectiveFov<float>(verticalFov, backbufferImage.extent.width, backbufferImage.extent.height, nearClippingPlaneDist, farClippingPlaneDist);
        glm::mat4 proj = glm::perspective<float>(verticalFov, aspectRatio, nearClippingPlaneDist, farClippingPlaneDist);
        proj[1][1] *= -1.f;
        return proj;
    }
};
