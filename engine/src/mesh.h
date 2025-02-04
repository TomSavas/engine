#pragma once 

#include <glm/glm.hpp>

#include <vector>

struct Mesh
{
    // .w = collision flag
    std::vector<glm::vec4> vertices;
    std::vector<uint32_t> indices;

    static Mesh cube(glm::vec3 position, glm::vec3 color, glm::vec3 scale = glm::vec3(1.f, 1.f, 1.f));
    static Mesh tesselatedPlane(glm::vec3 centerPosition, float width, float height, int widthSegments, int heightSegments);
};
