#include "scene.h"

#include "tracy/Tracy.hpp"

#include "GLFW/glfw3.h"

#include <atomic>
#include <algorithm>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>

glm::vec3 right(glm::quat mat) {
    return toMat4(mat) * glm::vec4(1.f, 0.f, 0.f, 0.f);
}

glm::vec3 up(glm::quat mat) {
    return toMat4(mat) * glm::vec4(0.f, 1.f, 0.f, 0.f);
}

glm::vec3 forward(glm::quat mat) {
    return toMat4(mat) * glm::vec4(0.f, 0.f, -1.f, 0.f);
}

static std::atomic<double> yOffsetAtomic;
void scrollCallback(GLFWwindow* window, double xoffset, double yOffset)
{
    yOffsetAtomic.store(yOffset);
}

Scene Scene::empty() 
{
    return Scene();
}

void Scene::update(float dt, GLFWwindow* window)
{
    ZoneScoped;

    static bool scrollCallbackSet = false;
    if (!scrollCallbackSet)
    {
        glfwSetScrollCallback(window, scrollCallback);
        scrollCallbackSet = true;
    }

    float scrollWheelChange = yOffsetAtomic.exchange(0.0);
    activeCamera.moveSpeed = std::max(0.f, activeCamera.moveSpeed + scrollWheelChange);

    static glm::dvec2 lastMousePos = glm::vec2(-1.f -1.f);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) 
    {
        static double radToVertical = .0;
        static double radToHorizon = .0;

        if (lastMousePos.x == -1.f) 
        {
            glfwGetCursorPos(window, &lastMousePos.x, &lastMousePos.y);
        }

        glm::dvec2 mousePos;
        glfwGetCursorPos(window, &mousePos.x, &mousePos.y);
        glm::dvec2 mousePosDif = mousePos - lastMousePos;
        lastMousePos = mousePos;

        radToVertical += mousePosDif.x * activeCamera.rotationSpeed / activeCamera.aspectRatio;
        while(radToVertical > glm::pi<float>() * 2) 
        {
            radToVertical -= glm::pi<float>() * 2;
        }
        while(radToVertical < -glm::pi<float>() * 2) 
        {
            radToVertical += glm::pi<float>() * 2;
        }

        radToHorizon -= mousePosDif.y * activeCamera.rotationSpeed;
        radToHorizon = std::min(std::max(radToHorizon, -glm::pi<double>() / 2 + 0.01), glm::pi<double>() / 2 - 0.01);

        // activeCamera.rotation = glm::eulerAngleYX(radToVertical, radToHorizon);
        activeCamera.rotation = glm::quat(glm::vec3(radToHorizon, -radToVertical, 0.f));
    }
    else
    {
        glfwGetCursorPos(window, &lastMousePos.x, &lastMousePos.y);
    }

    glm::vec3 dir(0.f, 0.f, 0.f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) 
    {
        dir += forward(activeCamera.rotation);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) 
    {
        dir -= forward(activeCamera.rotation);
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) 
    {
        dir += right(activeCamera.rotation);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) 
    {
        dir -= right(activeCamera.rotation);
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) 
    {
        dir += up(activeCamera.rotation);
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) 
    {
        dir -= up(activeCamera.rotation);
    }

    activeCamera.position += dir * activeCamera.moveSpeed * dt;
}
