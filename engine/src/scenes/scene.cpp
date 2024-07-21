#include "scene.h"

#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "physics/collisions.h"
#include "physics/xpbd.h"
#include "tracy/Tracy.hpp"

#include "GLFW/glfw3.h"
#include "imgui.h"

#include <atomic>
#include <algorithm>
#include <print>
#include <ranges>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <unordered_map>
#include <map>

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

void cpuSolveXpbd(float dt, int substeps, Scene& scene, std::vector<Model>& collisionModels)
{
    integratePositions(dt, scene.physicsEntities);

    // NOTE(savas): according to the XPBD paper
    auto collisions = detectCollisions(scene.models);
    solveCollisionConstraints(dt, scene.physicsEntities, collisions, collisionModels);

    const float substepDt = dt / static_cast<float>(substeps);
    for (int i = 0; i < substeps; ++i)
    {
        // Softbody
        solveEdgeConstraints(substepDt, scene.physicsEntities, scene.edges);
        solveVolumeConstraints(substepDt, scene.physicsEntities, scene.tetrahedra);
    }

    adjustVelocities(dt, scene.physicsEntities);
}

/*static*/ Model Model::cube(glm::vec3 position, glm::vec3 color, float scale, bool dynamic)
{
    return Model::cube(position, color, glm::vec3(scale), dynamic);
}

/*static*/ Model Model::cube(glm::vec3 position, glm::vec3 color, glm::vec3 scale, bool dynamic)
{
    Model cube = Model(glm::vec4(color, 1.f), dynamic);

    glm::vec4 p = glm::vec4(position, 0.f);
    glm::vec4 s = glm::vec4(scale, 1.f);
    cube.mesh->vertices = 
    { 
        // Bottom 4 corners
        // +z
        p + glm::vec4(-0.5f, -0.5f, 0.5f, 1.f) * s, p + glm::vec4(0.5f, -0.5f, 0.5f, 1.f) * s,
        // -z
        p + glm::vec4(-0.5f, -0.5f, -0.5f, 1.f) * s, p + glm::vec4(0.5f, -0.5f, -0.5f, 1.f) * s,
        // Top 4 corners
        // +z
        p + glm::vec4(-0.5f, 0.5f, 0.5f, 1.f) * s, p + glm::vec4(0.5f, 0.5f, 0.5f, 1.f) * s,
        // -z
        p + glm::vec4(-0.5f, 0.5f, -0.5f, 1.f) * s, p + glm::vec4(0.5f, 0.5f, -0.5f, 1.f) * s,
    };
    cube.mesh->indices = 
    {
        // front
        0, 5, 1, 
        0, 4, 5,
        // back
        2, 7, 6,
        2, 3, 7,
        // left
        2, 4, 0,
        2, 6, 4,
        // right
        1, 7, 3,
        1, 5, 7,
        // top
        4, 7, 5,
        4, 6, 7,
        // bottom
        2, 1, 3,
        2, 0, 1
    };
    // TEMP(savas): write a delauney tetrahedralizer
    cube.mesh->tetrahedraIndices = 
    {
        7, 0, 1, 5,
        0, 1, 3, 7,
        0, 2, 6, 7,
        7, 2, 3, 0,
        0, 4, 5, 7,
        7, 4, 6, 0
    };
    
    return cube;
}

// [(name, [(model, initial velocity)])]
static std::vector<std::pair<std::string, std::vector<std::pair<Model, glm::vec3>>>> testModels = 
{
    // {
    //     "2x1 fall BIG-small",
    //     {
    //         std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 5.f, 0.f), glm::vec3(1.f, 0.f, 0.f), 10.f, false), glm::vec3(0.f)),
    //         std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 10.f + 1.f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
    //     }
    // },
    {
        "1x1 stationary",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "1x1 fall",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 2.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "1x1 slide",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-2.f, 1.f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(5.f, 0.f, 0.f)),
        }
    },
    {
        "1x1 fast slide",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-10.f, 1.f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(20.f, 0.f, 0.f)),
        }
    },
    {
        "2x1 column",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.525f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 fall",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 2.f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 high fall",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 7.f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 offset",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.75f, 1.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 offset fall",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.75f, 2.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 offset high fall",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.75f, 7.f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 column big-small",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.25f, 0.f), glm::vec3(1.f, 0.f, 0.f), 2.5f), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 2.5f + 0.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 fall big-small",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.25, 0.f), glm::vec3(1.f, 0.f, 0.f), 2.5f), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 2.5f + 1.f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 high fall big-small",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.25, 0.f), glm::vec3(1.f, 0.f, 0.f), 2.5f), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 2.5f + 5.f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 offset big-small",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.25, 0.f), glm::vec3(1.f, 0.f, 0.f), 2.5f), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.25f + 0.25f, 2.5f + 0.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 offset fall big-small",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.25, 0.f), glm::vec3(1.f, 0.f, 0.f), 2.5f), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.25f + 0.25f, 2.5f + 1.f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 offset high fall big-small",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.25, 0.f), glm::vec3(1.f, 0.f, 0.f), 2.5f), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.25f + 0.25f, 2.5f + 5.f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "2x1 column small-big",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.f + 1.25f, 0.f), glm::vec3(1.f, 0.1f, 0.f), 2.5f), glm::vec3(0.f)),
        }
    },
    {
        "2x1 fall small-big",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.f + 1.25f + 1.f, 0.f), glm::vec3(1.f, 0.1f, 0.f), 2.5f), glm::vec3(0.f)),
        }
    },
    {
        "2x1 high fall small-big",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.f + 1.25f + 5.f, 0.f), glm::vec3(1.f, 0.1f, 0.f), 2.5f), glm::vec3(0.f)),
        }
    },
    {
        "2x1 offset small-big",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.25f, 1.f + 1.25f, 0.f), glm::vec3(1.f, 0.1f, 0.f), 2.5f), glm::vec3(0.f)),
        }
    },
    {
        "2x1 offset fall small-big",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.25f, 1.f + 1.25f + 1.f, 0.f), glm::vec3(1.f, 0.1f, 0.f), 2.5f), glm::vec3(0.f)),
        }
    },
    {
        "2x1 offset high fall small-big",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.25f, 1.f + 1.25f + 5.f, 0.f), glm::vec3(1.f, 0.1f, 0.f), 2.5f), glm::vec3(0.f)),
        }
    },
    {
        "4x1 offset",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-0.25f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.5f, 1.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-0.25f, 2.5f, 0.f), glm::vec3(1.f, 0.2f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.5f, 3.5f, 0.f), glm::vec3(1.f, 0.3f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "4x1 column",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 1.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 2.5f, 0.f), glm::vec3(1.f, 0.2f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 3.5f, 0.f), glm::vec3(1.f, 0.3f, 0.f)), glm::vec3(0.f)),
        }
    },
    {
        "4x3 wall + projectile",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-1.6f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-1.6f, 1.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-1.6f, 2.5f, 0.f), glm::vec3(1.f, 0.2f, 0.f)), glm::vec3(0.f)),
            // std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 3.5f, 0.f), glm::vec3(1.f, 0.3f, 0.f)), glm::vec3(0.f)),

            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-0.525f, 0.5f, 0.f), glm::vec3(1.f, 0.3f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-0.525f, 1.5f, 0.f), glm::vec3(1.f, 0.2f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-0.525f, 2.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
            // std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.f, 3.5f, 0.f), glm::vec3(1.f, 0.0f, 0.f)), glm::vec3(0.f)),

            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.525f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.525f, 1.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.525f, 2.5f, 0.f), glm::vec3(1.f, 0.2f, 0.f)), glm::vec3(0.f)),
            // std::pair<Model, glm::vec3>(Model::cube(glm::vec3(2.f, 3.5f, 0.f), glm::vec3(1.f, 0.3f, 0.f)), glm::vec3(0.f)),

            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.55f, 0.5f, 0.f), glm::vec3(1.f, 0.3f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.55f, 1.5f, 0.f), glm::vec3(1.f, 0.2f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.55f, 2.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
            // std::pair<Model, glm::vec3>(Model::cube(glm::vec3(3.f, 3.5f, 0.f), glm::vec3(1.f, 0.0f, 0.f)), glm::vec3(0.f)),

            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.f, 2.f, 20.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f, 0.f, -30.f)),
        }
    },
    {
        "4x4 wall",
        {
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-1.6f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-1.6f, 1.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-1.6f, 2.5f, 0.f), glm::vec3(1.f, 0.2f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-1.6f, 3.5f, 0.f), glm::vec3(1.f, 0.3f, 0.f)), glm::vec3(0.f)),

            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-0.525f, 0.5f, 0.f), glm::vec3(1.f, 0.3f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-0.525f, 1.5f, 0.f), glm::vec3(1.f, 0.2f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-0.525f, 2.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(-0.525f, 3.5f, 0.f), glm::vec3(1.f, 0.0f, 0.f)), glm::vec3(0.f)),

            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.525f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.525f, 1.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.525f, 2.5f, 0.f), glm::vec3(1.f, 0.2f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(0.525f, 3.5f, 0.f), glm::vec3(1.f, 0.3f, 0.f)), glm::vec3(0.f)),

            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.55f, 0.5f, 0.f), glm::vec3(1.f, 0.3f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.55f, 1.5f, 0.f), glm::vec3(1.f, 0.2f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.55f, 2.5f, 0.f), glm::vec3(1.f, 0.1f, 0.f)), glm::vec3(0.f)),
            std::pair<Model, glm::vec3>(Model::cube(glm::vec3(1.55f, 3.5f, 0.f), glm::vec3(1.f, 0.0f, 0.f)), glm::vec3(0.f)),
        }
    },
};

void initTestModels(Scene& scene, std::vector<std::pair<Model, glm::vec3>>& models)
{
    scene.collisionModels.clear();

    scene.models.clear();
    scene.physicsEntities.clear();

    scene.edges.clear();
    scene.tetrahedra.clear();

    for (auto& model : models)
    {
        Model copiedModel(model.first);
        scene.models.push_back(copiedModel);
    }

    // Model ground = Model::cube(glm::vec3(0.f, -5.15f, 0.f), glm::vec3(0.1f), 10.f);
    // ground.dynamic = false;
    // scene.models.push_back(ground);

    int modelIndex = 0;
    for (auto& model : scene.models)
    {
        int modelVertexOffset = scene.physicsEntities.size();

        glm::mat4 rotation = glm::mat4(1.f);
        // rotation = glm::rotate(rotation, (float)M_PI/2.f, glm::vec3(1.f, 1.f, 1.f));

        for (int i = 0; i < model.mesh->vertices.size(); ++i)
        {
            Particle particle = 
            {
                // .velocity = glm::vec3(0.f),
                .velocity = models[modelIndex].second,
                .position = model.mesh->vertices[i] * rotation,
                .previousPosition = model.mesh->vertices[i] * rotation,
                .mass = 1.f,
                .invMass = 1.f,
                // .edgeCompliance = 0.0001f + (modelIndex) * 0.005f,
                // .edgeCompliance = 0.0000000000001f + (modelIndex) * 0.005f,
                // .edgeCompliance = 0.000f,
                .edgeCompliance = 0.00008f,
                // .edgeCompliance = 0.00008f + (modelIndex) * 0.005f,
                .volumeCompliance = 0.f,
                .dynamic = model.dynamic,
            };
            model.mesh->particleIndices.push_back(scene.physicsEntities.size());

            scene.physicsEntities.push_back(particle);
        }

        std::unordered_map<IndexSet, Edge> edges;
        for (int i = 0; i < model.mesh->indices.size(); i += 3)
        {
            for (int j = 0; j < 3; j++)
            {
                int aInd = model.mesh->indices[i + j];
                int bInd = model.mesh->indices[i + (j + 1) % 3];
                glm::vec3 a = model.mesh->vertices[aInd];
                glm::vec3 b = model.mesh->vertices[bInd];
                edges[IndexSet({aInd + modelVertexOffset, bInd + modelVertexOffset})] = Edge 
                {
                    .restDistance = glm::distance(a, b),
                    .lambda = 0.f,
                };
            }
        }
        scene.edges.push_back(edges);

        std::unordered_map<IndexSet, Tetrahedra> tetrahedra;
        for (int i = 0; i < model.mesh->tetrahedraIndices.size(); i += 4)
        {
            glm::vec3 a = model.mesh->vertices[model.mesh->tetrahedraIndices[i + 0]];
            glm::vec3 b = model.mesh->vertices[model.mesh->tetrahedraIndices[i + 1]];
            glm::vec3 c = model.mesh->vertices[model.mesh->tetrahedraIndices[i + 2]];
            glm::vec3 d = model.mesh->vertices[model.mesh->tetrahedraIndices[i + 3]];
            float restVolume = glm::dot(glm::cross(b - a, c - a), d - a) / 6.f;
            IndexSet indexSet(
                {
                    static_cast<int>(model.mesh->tetrahedraIndices[i + 0] + modelVertexOffset),
                    static_cast<int>(model.mesh->tetrahedraIndices[i + 1] + modelVertexOffset),
                    static_cast<int>(model.mesh->tetrahedraIndices[i + 2] + modelVertexOffset),
                    static_cast<int>(model.mesh->tetrahedraIndices[i + 3] + modelVertexOffset)
                });
            tetrahedra[indexSet] = Tetrahedra 
            {
                .restVolume = restVolume,
                .lambda = 0.f,
            };
        }
        scene.tetrahedra.push_back(tetrahedra);

        modelIndex++;
    }
}

Scene Scene::empty() 
{
    Scene empty = Scene("empty");
    // empty.activeCamera.rotation = glm::inverse(glm::lookAt(empty.activeCamera.position, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f)));

    initTestModels(empty, testModels[0].second);

    return empty;
}

struct TestResult 
{
    enum Result {
        Pass,
        Fail,
        Skipped
    };
    
    std::string sceneName;
    bool running;
    Result result;  
    float startTimeMs;
    float runTimeMs;
    float errorToReference;
};

void Scene::update(float dt, float currentTimeMs, GLFWwindow* window)
{
    updateFreeCamera(dt, window, activeCamera);

    static std::string selectedMode = testModels.begin()->first;
    static int selectedScene = 0;
    static int finishAutoTestOnSceneIndex = 0;
    static bool autoTestMode = false;
    static bool pauseOnCollision = false;
    constexpr int defaultAutoTestGraceFrames = 20;
    static int autoTestGraceFrames = defaultAutoTestGraceFrames;
    static std::vector<TestResult> testResults;
    bool skipCurrentTest = false;

    if (ImGui::Begin("Scene"))
    {
        ImGui::Text("Simulation running: ");
        ImGui::SameLine();
        if (worldPaused)
        {
            ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "false");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "true");
        }

        ImGui::Separator();

        ImGui::Checkbox("Show collisions", &collisionModelsVisible);
        ImGui::Checkbox("Pause on collision", &pauseOnCollision);

        ImGui::Separator();
        
        std::vector<const char*> scenes;
        for (auto& testModel : testModels)
        {
            scenes.push_back(testModel.first.c_str());
        }

        ImGui::Text("Current scene");
        ImGui::SameLine();
        ImGui::BeginDisabled(autoTestMode);
        if (ImGui::Combo("##current scene", &selectedScene, scenes.data(), scenes.size())) 
        {
            worldPaused = true;
            autoTestMode = false;
            initTestModels(*this, testModels[selectedScene].second);
        }
        ImGui::EndDisabled();

        ImGui::Separator();

        if (ImGui::Checkbox("Autotest", &autoTestMode))
        {
            if (autoTestMode)
            {
                autoTestGraceFrames = defaultAutoTestGraceFrames;
                finishAutoTestOnSceneIndex = selectedScene;
                worldPaused = false;

                testResults.clear();

                testResults.push_back(TestResult {
                    .sceneName = testModels[selectedScene].first,
                    .running = true,
                    .result = TestResult::Result::Skipped,
                    .startTimeMs = currentTimeMs,
                    .runTimeMs = 0.f,
                    .errorToReference = 0.f,
                });
            }
        }

        float totalTimeMs = std::ranges::fold_right(testResults.begin(), testResults.end(), 0.f,
            [](TestResult& testResult, float sum) { return sum + testResult.runTimeMs; });
        ImGui::Text("Total run time: %lf ms", totalTimeMs);

        constexpr ImU32 runningColor = IM_COL32(100, 100, 0, 255);
        constexpr ImU32 successColor = IM_COL32(0, 100, 0, 255);
        constexpr ImU32 failureColor = IM_COL32(100, 0, 0, 255);
        constexpr ImU32 partialSuccessColor = IM_COL32(100, 50, 0, 255);
        constexpr ImU32 skipColor = IM_COL32(50, 50, 50, 255);

        int successCount = std::count_if(testResults.begin(), testResults.end(),
            [](TestResult& testResult) { return testResult.result == TestResult::Result::Pass; });
        int failureCount = std::count_if(testResults.begin(), testResults.end(),
            [](TestResult& testResult) { return testResult.result == TestResult::Result::Fail; });
        bool partialSuccess = testResults.size() != successCount;

        skipCurrentTest = ImGui::Button("Skip current");

        ImGui::PushStyleColor(ImGuiCol_Header, autoTestMode ? runningColor : (partialSuccess ? partialSuccessColor : (successCount ? successColor : failureColor)));
        ImGui::SetNextItemOpen(true);

        std::string label = std::format("Test results {} / {} (+:{}, -:{})", testResults.size(), testModels.size(), successCount, failureCount);
        if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_Framed))
        {
            for (auto& testResult : testResults)
            {
                if (testResult.running)
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, runningColor);
                }
                else 
                {
                    ImU32 color = skipColor;
                    switch(testResult.result)
                    {
                    case TestResult::Result::Pass:
                        color = successColor;
                        break;
                    case TestResult::Result::Fail:
                        color = failureColor;
                        break;
                    case TestResult::Result::Skipped:
                        color = skipColor;
                        break;
                    }
                    ImGui::PushStyleColor(ImGuiCol_Header, color);
                }

                if (ImGui::CollapsingHeader(testResult.sceneName.c_str()))
                {
                    ImGui::Text("Error: %lf", testResult.errorToReference);
                    ImGui::Text("Test run time: %lf ms", testResult.runTimeMs);

                    ImGui::Button("Save as ground truth");
                }

                ImGui::PopStyleColor();
            }

            ImGui::TreePop();
        }
        ImGui::PopStyleColor();
        
        ImGui::End();
    }

    if (autoTestMode)
    {
        bool noSignificantPhysicsMovement = true;
        for (int i = 0; i < physicsEntities.size() && noSignificantPhysicsMovement; ++i)
        {
            noSignificantPhysicsMovement &= glm::length(physicsEntities[i].velocity) < 0.01f;
        }

        for (auto& testResult : testResults)
        {
            if (testResult.running)
            {
                testResult.runTimeMs += dt;
            }
        }
        
        if (noSignificantPhysicsMovement || skipCurrentTest)
        {
            autoTestGraceFrames--;
            if (autoTestGraceFrames < 0 || skipCurrentTest)
            {
                testResults.back().running = false;
                // TODO(savas): calculate errorToReference and set pass
                testResults.back().result = skipCurrentTest ? TestResult::Result::Skipped : TestResult::Result::Pass;
                selectedScene = (selectedScene + 1) % testModels.size();

                if (selectedScene == finishAutoTestOnSceneIndex)
                {
                    selectedScene = (selectedScene - 1) % testModels.size();
                    worldPaused = true;
                    autoTestMode = false;
                }
                else 
                {
                    initTestModels(*this, testModels[selectedScene].second);
                    autoTestGraceFrames = defaultAutoTestGraceFrames;

                    testResults.push_back(TestResult {
                        .sceneName = testModels[selectedScene].first,
                        .running = true,
                        .result = TestResult::Result::Skipped,
                        .startTimeMs = currentTimeMs,
                        .runTimeMs = 0.f,
                        .errorToReference = 0.f
                    });
                }
            }
        }
    }

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) 
    {
        // initTestModels(*this);
        initTestModels(*this, testModels[selectedScene].second);
    }

    static bool pauseKeyReleased = true;
    int pauseKeyState = glfwGetKey(window, GLFW_KEY_P);
    if (pauseKeyState == GLFW_RELEASE)
    {
        pauseKeyReleased = true;
    }
    if (pauseKeyState == GLFW_PRESS && pauseKeyReleased) 
    {
        worldPaused = !worldPaused;
        pauseKeyReleased = false;
    }

    static bool stepWasReleased = true;
    bool stepPressed = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    bool step = stepPressed && stepWasReleased;
    if (!worldPaused || step)
    {
        // NOTE(savas): reset collision flags
        for (auto& model : models)
        {
            for (int i = 0; i < model.mesh->vertices.size(); ++i)
            {
                model.mesh->vertices[i].w = 0.f;
            }
        }

        collisionModels.clear();
        cpuSolveXpbd(dt, 20, *this, collisionModels);

        if (pauseOnCollision && collisionModels.size())
        {
            worldPaused = true;
        }
    }

    stepWasReleased = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_RELEASE;

    int particleIndex = 0;
    for (auto& model : models)
    {
        for (int i = 0; i < model.mesh->vertices.size(); ++i)
        {
            // NOTE(savas): we pass through .w as a collision flag from when solving collisions with XPBD
            model.mesh->vertices[i] = glm::vec4(physicsEntities[particleIndex].position, model.mesh->vertices[i].w);
            particleIndex++;
        }
    }
}
