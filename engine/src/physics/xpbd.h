#pragma once

#include "physics/collisions.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <initializer_list>

static constexpr glm::vec3 gravity = glm::vec3(0.f, -10.0f, 0.f);

struct Particle
{
    glm::vec3 velocity;
    glm::vec3 localVelocity;
    glm::vec3 position;
    glm::vec3 previousPosition;
    float mass;
    float invMass;
    float edgeCompliance;
    float volumeCompliance;
    bool dynamic;
};

template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

// TEMP(savas): to allow the map
struct IndexSet 
{
    std::vector<int> indices;

    IndexSet(std::initializer_list<int> indices) : indices(indices) 
    {
    }

    bool operator==(const IndexSet &other) const
    { 
        return indices.size() == other.indices.size() &&
            std::includes(indices.begin(), indices.end(), other.indices.begin(), other.indices.end());
    }
};

template<>
struct std::hash<IndexSet>
{
    std::size_t operator() (IndexSet const &indices) const
    {
        std::size_t hash = std::hash<int>{}(0);
        for (auto& index : indices.indices)
        {
            hash_combine(hash, index);
        }

        return hash;
    }
};

struct Edge 
{
    float restDistance;
    float lambda;
};

struct Tetrahedra
{
    float restVolume;
    float lambda;
};

void integratePositions(float dt, std::vector<Particle>& particles);
void solveEdgeConstraints(float dt, std::vector<Particle>& particles, std::vector<std::unordered_map<IndexSet, Edge>>& edges);
void solveVolumeConstraints(float dt, std::vector<Particle>& particles, std::vector<std::unordered_map<IndexSet, Tetrahedra>>& tetrahedra);
void solveCollisionConstraints(float substepDt, std::vector<Particle>& particles, std::vector<Collision>& collisions, std::vector<Model>& collisionModels);
void adjustVelocities(float dt, std::vector<Particle>& particles);
