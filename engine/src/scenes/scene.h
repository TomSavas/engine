#pragma once

#include "camera.h"
#include "physics/xpbd.h"

#include <string>
#include <unordered_set>
#include <vulkan/vulkan.h> // TEMP(savas): remo with GPUMeshBuffers

class GLFWwindow;

struct Mesh
{
    // .w = collision flag
    std::vector<glm::vec4> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> tetrahedraIndices;

    std::vector<uint32_t> particleIndices;
    uint32_t offsetInParticles;

    Mesh() {}

    Mesh(const Mesh& other) : 
        vertices(other.vertices), indices(other.indices),
        tetrahedraIndices(other.tetrahedraIndices), particleIndices(other.particleIndices), 
        offsetInParticles(other.offsetInParticles)
    {
    }

    Mesh(Mesh&& other) : 
        vertices(other.vertices), indices(other.indices),
        tetrahedraIndices(other.tetrahedraIndices), particleIndices(other.particleIndices), 
        offsetInParticles(other.offsetInParticles)
    {
    }
};

struct Model
{
    Mesh* mesh;

    glm::vec3 color;
    bool dynamic;
    std::unordered_set<uint32_t> staticVertices;

    //glm::vec3 position;
    // TODO(savas): add rotation

    static Model cube(glm::vec3 position, glm::vec3 color, float scale = 1.f, bool dynamic = true);
    static Model cube(glm::vec3 position, glm::vec3 color, glm::vec3 scale, bool dynamic = true);

    static Model tesselatedPlane(glm::vec3 centerPosition, float width, float height, int widthSegments, int heightSegments);

    Model(glm::vec3 color, bool dynamic = true) : mesh(new Mesh()), color(color), dynamic(dynamic) {}

    Model(const Model& other)
        : mesh(new Mesh(*other.mesh)), color(other.color), dynamic(other.dynamic), staticVertices(other.staticVertices)
    {
    }

    Model(Model&& other)
        : mesh(other.mesh), color(other.color), dynamic(other.dynamic), staticVertices(other.staticVertices)
    {
        other.mesh = nullptr;
    }

    Model& operator=(const Model& other)
    {
        // mesh = new Mesh(*other.mesh);
        mesh = other.mesh;
        color = other.color;
        dynamic = other.dynamic;
        staticVertices = other.staticVertices;

        return *this;
    }

    std::vector<Model> copyTriangles();
};


struct Scene 
{
    std::string name;

    Camera& activeCamera;
    Camera mainCamera;
    Camera debugCamera;

    std::vector<Particle> physicsEntities;
    std::vector<std::unordered_map<IndexSet, Edge>> edges;
    std::vector<std::unordered_map<IndexSet, Tetrahedra>> tetrahedra;
    std::vector<Model> models;
    std::vector<Model> collisionModels;
    //Model model;

    bool worldPaused = true;
    bool collisionModelsVisible = false;

    static Scene empty();

    void update(float dt, float currentTimeMs, GLFWwindow* window);

private:
    Scene(std::string name) : name(name), activeCamera(mainCamera) {}
};
