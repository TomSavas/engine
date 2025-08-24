#pragma once

#include "engine.h"

#include <glm/glm.hpp>
#include <string>

struct Vertex
{
    union
    {
        struct
        {
            f32 pos[4];
            f32 uv[4];
            f32 normal[4];
            f32 tangent[4];
        };
        f32 raw[4 * 4];
    };
};

struct Mesh
{
    std::string debugName;

    i32 vertexOffset;
    i32 vertexCount;

    i32 indexOffset;
    i32 indexCount;

    glm::vec3 aabbMin;
    glm::vec3 aabbMax;

    // TODO: Move out this to a standalone material
    // i32 materialIndex;
    i16 albedoTexture;
    i16 normalTexture;
    i16 bumpTexture;
};
