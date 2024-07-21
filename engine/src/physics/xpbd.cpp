#include "physics/xpbd.h"
#include "physics/collisions.h"
#include "scenes/scene.h"

#include "glm/geometric.hpp"
#include "glm/glm.hpp"

#include <limits>
#include <print>
#include <random>

void integratePositions(float dt, std::vector<Particle>& particles)
{
    for (auto& particle : particles)
    {
        particle.localVelocity = particle.velocity + gravity * dt;
        particle.previousPosition = particle.position;
        particle.position += particle.localVelocity * dt;

        if (particle.position.y < 0.0)
        {
            particle.position = particle.previousPosition;
            particle.position.y = 0.f;
        }
    }
}

void solveEdgeConstraints(float dt, std::vector<Particle>& particles, std::vector<std::unordered_map<IndexSet, Edge>>& edges)
{
    // NOTE(savas): undamped version
    for (auto& edgeSet : edges)
    {
        for (auto& [edgeVertexIndices, edge] : edgeSet)
        {
            auto indexIt = edgeVertexIndices.indices.begin();
            Particle& a = particles[*(indexIt++)];
            // Particle& b = particles[*edgeVertexIndices.indices.rbegin()];
            Particle& b = particles[*(indexIt++)];

            // TEMP(savas): testing stiffnesses
            float alpha = (a.edgeCompliance + b.edgeCompliance) / (2 * dt * dt);

            float w = a.invMass + b.invMass;
            // TODO(savas): early out if w == 0

            glm::vec3 grad = a.position - b.position;
            float dist = glm::length(grad);
            if (-std::numeric_limits<float>::epsilon() <= dist && dist <= std::numeric_limits<float>::epsilon())
                continue;
            grad /= dist;
            // TODO(savas): early out if dist == 0
        
            float c = dist - edge.restDistance;
            // float deltaLambda = -c / (w + alpha + std::numeric_limits<float>::epsilon());
            // float deltaLambda = (-c - alpha * edge.lambda) / (w + alpha);
            float deltaLambda = (-c) / (w + alpha);

            a.position += grad * deltaLambda * a.invMass;// * (a.dynamic ? 1.f : 0.f);
            b.position -= grad * deltaLambda * b.invMass;// * (b.dynamic ? 1.f : 0.f);
            edge.lambda += deltaLambda;
        }
    }
}

void solveVolumeConstraints(float dt, std::vector<Particle>& particles, std::vector<std::unordered_map<IndexSet, Tetrahedra>>& tetrahedra)
{
    // NOTE(savas): undamped version
    for (auto& tetrahedraSet : tetrahedra)
    {
        for (auto& [tetrahedraVertexIndices, tetrahedra] : tetrahedraSet)
        {
            auto indexIt = tetrahedraVertexIndices.indices.begin();
            Particle& a = particles[*(indexIt++)];
            Particle& b = particles[*(indexIt++)];
            Particle& c = particles[*(indexIt++)];
            Particle& d = particles[*(indexIt++)];
 
            float alpha = (a.volumeCompliance + b.volumeCompliance + c.volumeCompliance + d.volumeCompliance) / (4 * dt * dt);

            glm::vec3 aGrad = glm::cross(d.position - b.position, c.position - b.position);
            glm::vec3 bGrad = glm::cross(c.position - a.position, d.position - a.position);
            glm::vec3 cGrad = glm::cross(d.position - a.position, b.position - a.position);
            glm::vec3 dGrad = glm::cross(b.position - a.position, c.position - a.position);

            float w = 0;
            w += a.invMass * glm::length(aGrad);
            w += b.invMass * glm::length(bGrad);
            w += c.invMass * glm::length(cGrad);
            w += d.invMass * glm::length(dGrad);
            // TODO(savas): early out if w == 0

            if (-std::numeric_limits<float>::epsilon() <= w && w <= std::numeric_limits<float>::epsilon())
                continue;

            float volume = glm::dot(glm::cross(b.position - a.position, c.position - a.position), d.position - a.position) / 6.f;
            float C = volume - tetrahedra.restVolume;
            float deltaLambda = (-C) / (w + alpha);

            a.position += aGrad * deltaLambda * a.invMass;// * (a.dynamic ? 1.f : 0.f);
            b.position += bGrad * deltaLambda * b.invMass;// * (b.dynamic ? 1.f : 0.f);
            c.position += cGrad * deltaLambda * c.invMass;// * (c.dynamic ? 1.f : 0.f);
            d.position += dGrad * deltaLambda * d.invMass;// * (d.dynamic ? 1.f : 0.f);

            tetrahedra.lambda += deltaLambda;
        }
    }

    // std::println("{} (ref: {})", totalVolume, restTotalVolume);
}

void solveCollisionConstraints(float dt, std::vector<Particle>& particles, std::vector<Collision>& collisions, std::vector<Model>& collisionModels)
{
    // NOTE(savas): utterly horrible
    const auto handler = [&particles, &collisionModels](Model& collider, Model& otherCollider, glm::vec3 color)
        {
            glm::vec3 centerOfMass = glm::vec3(0.f);
            for (int i = 0; i < collider.mesh->vertices.size(); i++)
            {
                centerOfMass += collider.mesh->vertices[i];
            }
            centerOfMass /= collider.mesh->vertices.size();

            // NOTE(savas): ultra fucking hack
            for (int i = 0; i < collider.mesh->particleIndices.size(); i++)
            {
                Particle& particle = particles[collider.mesh->particleIndices[i]];

                Model fakeParticleModel = Model::cube(glm::vec3(), color, 1.f);
                fakeParticleModel.mesh->vertices = std::vector<glm::vec4>(collider.mesh->vertices);

                const glm::vec3 offset = (centerOfMass - particle.position) * 0.5f;
                for (auto& vert : fakeParticleModel.mesh->vertices)
                {
                    glm::vec3 n = centerOfMass - glm::vec3(vert);
                    vert = vert + glm::vec4(n * 0.5f, 0.f) - glm::vec4(offset, 0.f);
                }

                auto gjkResult = naiveGjk(&otherCollider, &fakeParticleModel);
                if (gjkResult.collides)
                {
                    auto col = epa(&otherCollider, &fakeParticleModel, gjkResult);
                    if (col.a == nullptr || col.b == nullptr)
                    {
                        continue;
                    }
                    col.collisionVec *= 100.f;

                    float colLen = glm::length(col.collisionVec);
                    Model collisionNormalModel = Model::cube(glm::vec3(0.f), glm::vec3(1.f, 1.f, 0.f) * color, glm::vec3(1.f));
                    // rotate
                    auto rotationAxis = glm::normalize(glm::cross(glm::normalize(col.collisionVec), glm::vec3(0.f, 1.f, 0.f)));
                    for (auto& vert : collisionNormalModel.mesh->vertices)
                    {
                        glm::mat4 model = glm::mat4(1.f);
                        model = glm::scale(model, glm::vec3(0.01f, colLen, 0.01f));
                        model = glm::rotate(model, glm::acos(glm::dot(glm::normalize(col.collisionVec), glm::vec3(0.f, 1.f, 0.f))), rotationAxis);
                        vert.w = 1.f;
                        vert = vert * model + glm::vec4(particle.position + col.collisionVec / 2.f, 0.f);
                    }

                    collisionModels.push_back(collisionNormalModel);
            
                    particle.position += col.collisionVec / 100.f * 0.5f; 
                    std::println("{} {} {}", col.collisionVec.x, col.collisionVec.y, col.collisionVec.z);

                    // NOTE(savas): flag for colouring vertices in the shader
                    collider.mesh->vertices[i].w = 112233.f;
                    fakeParticleModel.color = glm::vec3(1.f, 0.f, 1.f);
                }

                collisionModels.push_back(fakeParticleModel);
            }
        };

    for (auto& collision : collisions)
    {
        if (collision.a != nullptr && collision.b != nullptr)
        {
            handler(*collision.a, *collision.b, glm::vec3(1.f, 1.f, 1.f));
            handler(*collision.b, *collision.a, glm::vec3(0.5f, 0.5f, 0.5f));
        }
    }
}

void adjustVelocities(float dt, std::vector<Particle>& particles)
{
    for (auto& particle : particles)
    {
        particle.velocity = (particle.position - particle.previousPosition) / dt;
    }
}
