#include "scene.h"

#include "tracy/Tracy.hpp"

#include "GLFW/glfw3.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <atomic>
#include <algorithm>
#include <print>

glm::vec3 right(glm::mat4 mat) {
    return mat * glm::vec4(1.f, 0.f, 0.f, 0.f);
}

glm::vec3 up(glm::mat4 mat) {
    return mat * glm::vec4(0.f, 1.f, 0.f, 0.f);
}

glm::vec3 forward(glm::mat4 mat) {
    return mat * glm::vec4(0.f, 0.f, -1.f, 0.f);
}

static std::atomic<double> yOffsetAtomic;
void scrollCallback(GLFWwindow* window, double xoffset, double yOffset)
{
    yOffsetAtomic.store(yOffset);
}

void updateFreeCamera(float dt, GLFWwindow* window, Camera& camera)
{
    ZoneScoped;

    static bool scrollCallbackSet = false;
    if (!scrollCallbackSet)
    {
        glfwSetScrollCallback(window, scrollCallback);
        scrollCallbackSet = true;
    }

    float scrollWheelChange = yOffsetAtomic.exchange(0.0);
    camera.moveSpeed = std::max(0.f, camera.moveSpeed + scrollWheelChange);

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

        radToVertical += mousePosDif.x * camera.rotationSpeed / camera.aspectRatio;
        while(radToVertical > glm::pi<float>() * 2) 
        {
            radToVertical -= glm::pi<float>() * 2;
        }
        while(radToVertical < -glm::pi<float>() * 2) 
        {
            radToVertical += glm::pi<float>() * 2;
        }

        radToHorizon -= mousePosDif.y * camera.rotationSpeed;
        radToHorizon = std::min(std::max(radToHorizon, -glm::pi<double>() / 2 + 0.01), glm::pi<double>() / 2 - 0.01);

        camera.rotation = glm::eulerAngleYX(-radToVertical, radToHorizon);
    }
    else
    {
        glfwGetCursorPos(window, &lastMousePos.x, &lastMousePos.y);
    }

    glm::vec3 dir(0.f, 0.f, 0.f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) 
    {
        dir += forward(camera.rotation);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) 
    {
        dir -= forward(camera.rotation);
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) 
    {
        dir += right(camera.rotation);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) 
    {
        dir -= right(camera.rotation);
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) 
    {
        dir += up(camera.rotation);
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) 
    {
        dir -= up(camera.rotation);
    }

    camera.position += dir * camera.moveSpeed * dt;
    
}

void Scene::update(float dt, float currentTimeMs, GLFWwindow* window)
{
    updateFreeCamera(dt, window, activeCamera);
}
