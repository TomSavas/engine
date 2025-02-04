#include "mesh.h"

/*static*/ Mesh Mesh::cube(glm::vec3 position, glm::vec3 color, glm::vec3 scale)
{
    Mesh cube;
    
    glm::vec4 p = glm::vec4(position, 0.f);
    glm::vec4 s = glm::vec4(scale, 1.f);
    cube.vertices = 
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
    cube.indices = 
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
    
    return cube;
}

/*static*/ Mesh Mesh::tesselatedPlane(glm::vec3 centerPosition, float width, float height, int widthSegments, int heightSegments)
{
    Mesh plane;

    float segmentWidth = width / widthSegments;
    float segmentHeigth = height / heightSegments;

    glm::vec3 topLeftPos = centerPosition - glm::vec3(width, height, 0.f) * 0.5f;
    glm::vec3 widthOffset = glm::vec3(segmentWidth, 0.f, 0.f);
    glm::vec3 heightOffset = glm::vec3(0.f, segmentHeigth, 0.f);
    for (int y = 0; y <= heightSegments; y++)
    {
        for (int x = 0; x <= widthSegments; x++)
        {
            glm::vec4 vertexPos = glm::vec4(topLeftPos + widthOffset * static_cast<float>(x) + heightOffset * static_cast<float>(y), 0.f);
            plane.vertices.push_back(vertexPos);
        }
    }

    const uint32_t verticesPerWidth = widthSegments + 1;
    // NOTE(savas): we are connecting current row with the one bellow, so no need to process the last row
    for (int y = 0; y < heightSegments; y++)
    {
        // NOTE(savas): we are processing current col with one on the right, so no need to process the last col
        for (int x = 0; x < widthSegments; x++)
        {
            const uint32_t topLeftVertex = y * verticesPerWidth + x;
            const uint32_t bottomLeftVertex = (y + 1) * verticesPerWidth + x;

            plane.indices.insert(plane.indices.end(), 
                {
                    topLeftVertex, bottomLeftVertex + 1, topLeftVertex + 1,
                    topLeftVertex, bottomLeftVertex, bottomLeftVertex + 1,
                });
        }
    }

    return plane;
}
