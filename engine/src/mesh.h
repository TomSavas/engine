#pragma once

#include <glm/glm.hpp>
#include <string>

struct Vertex
{
    union
    {
        struct
        {
            float pos[4];
            float uv[4];
            float normal[4];
            float tangent[4];
        };
        float raw[4 * 4];
    };
};

struct Texture;
struct Mesh
{
    std::string debugName;

    int vertexOffset;
    int vertexCount;

    int indexOffset;
    int indexCount;

    glm::vec3 aabbMin;
    glm::vec3 aabbMax;

    // Move out this to a standalone material
    // int materialIndex;
    int albedoTexture;
    int normalTexture;
};
