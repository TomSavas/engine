#pragma once

#include "camera.h"

class GLFWwindow;

struct Scene 
{
    Camera& activeCamera;
    Camera mainCamera;
    Camera debugCamera;

    static Scene empty();
    void update(float dt, GLFWwindow* window);
private:
    Scene() : activeCamera(mainCamera) {}
};
