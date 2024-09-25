#include "physics/xpbd.h"
#include "physics/collisions.h"
#include "scenes/scene.h"

#include "glm/geometric.hpp"
#include "glm/glm.hpp"

#include <limits>
#include <print>
#include <random>

bool isDynamic(Particle& particle)
{
    return particle.dynamic < 5;
}

void integratePositions(float dt, std::vector<Particle>& particles)
{
    for (auto& particle : particles)
    {
        if (isDynamic(particle))
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
        particle.dynamic = std::min(5, particle.dynamic + particle.dynamic);
    }
}

void solveEdgeConstraints(float dt, std::vector<Particle>& particles, std::vector<std::unordered_map<IndexSet, Edge>>& edges, float edgeCompliance, float dampingStiffness)
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
            // float alpha = (a.edgeCompliance + b.edgeCompliance) / (2 * dt * dt);
            float alpha = edgeCompliance / (dt * dt);
            //float beta = dampingStiffness / (dt * dt);
            float beta = dt * dt * dampingStiffness;
            float gamma = (alpha * beta) / dt;

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
            if (-std::numeric_limits<float>::epsilon() <= (w + alpha) && (w + alpha) <= std::numeric_limits<float>::epsilon())
                continue;
            //float deltaLambda = (-c) / (w + alpha);
            //float deltaLambda = (-c - alpha * gamma - gamma * dist) / ((1.f + gamma) * w + alpha);
            float gradDotPosDiff = glm::dot(grad, a.position - a.previousPosition);
            float deltaLambda = (-c - gamma * gradDotPosDiff) / ((1.f + gamma) * w + alpha);

            // a.position += grad * deltaLambda * a.invMass;// * (isDynamic(a) ? 1.f : 0.f);
            // b.position -= grad * deltaLambda * b.invMass;// * (isDynamic(b) ? 1.f : 0.f);
            a.position += grad * deltaLambda * a.invMass * (isDynamic(a) ? 1.f : 0.f);
            b.position -= grad * deltaLambda * b.invMass * (isDynamic(b) ? 1.f : 0.f);
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

            // if (-std::numeric_limits<float>::epsilon() <= w && w <= std::numeric_limits<float>::epsilon())
                // continue;

            float volume = glm::dot(glm::cross(b.position - a.position, c.position - a.position), d.position - a.position) / 6.f;
            float C = volume - tetrahedra.restVolume;
            float deltaLambda = (-C) / (w + alpha);

            a.position += aGrad * deltaLambda * a.invMass * (isDynamic(a) ? 1.f : 0.f);
            b.position += bGrad * deltaLambda * b.invMass * (isDynamic(b) ? 1.f : 0.f);
            c.position += cGrad * deltaLambda * c.invMass * (isDynamic(c) ? 1.f : 0.f);
            d.position += dGrad * deltaLambda * d.invMass * (isDynamic(d) ? 1.f : 0.f);

            tetrahedra.lambda += deltaLambda;
        }
    }

    // std::println("{} (ref: {})", totalVolume, restTotalVolume);
}

void solveCollisionConstraints(float dt, std::vector<Particle>& particles, std::vector<Collision>& collisions, std::vector<Model>& collisionModels)
{
    // NOTE(savas): utterly horrible
    const auto handler = [&](Model& collider, Model& otherCollider, glm::vec3 color)
        {
            glm::vec3 centerOfMass = glm::vec3(0.f);
            for (int i = 0; i < collider.mesh->vertices.size(); i++)
            {
                // centerOfMass += collider.mesh->vertices[i];
                // centerOfMass += particles[collider.mesh->offsetInParticles + i].position;
                centerOfMass += particles[collider.mesh->particleIndices[i]].position;
            }
            centerOfMass /= collider.mesh->vertices.size();

            // NOTE(savas): ultra fucking hack
            for (int i = 0; i < collider.mesh->particleIndices.size(); i++)
            {
                Particle& particle = particles[collider.mesh->particleIndices[i]];

                std::vector<glm::vec4> fakeParticleModelVerts;
                // Model fakeParticleModel = Model::cube(glm::vec3(), color, 1.f);
                for (int j = 0; j < collider.mesh->vertices.size(); j++)
                {
                    // fakeParticleModel.mesh->vertices.push_back(glm::vec4(particles[collider.mesh->offsetInParticles + i].position, 0));
                    // fakeParticleModelVerts.push_back(glm::vec4(particles[collider.mesh->offsetInParticles + j].position, 0));
                    fakeParticleModelVerts.push_back(glm::vec4(particles[collider.mesh->particleIndices[j]].position, 0));
                }
                //fakeParticleModel.mesh->vertices = std::vector<glm::vec4>(collider.mesh->vertices);

                const glm::vec3 offset = (centerOfMass - particle.position) * 0.5f;
                // for (auto& vert : fakeParticleModel.mesh->vertices)
                for (int j = 0; j < fakeParticleModelVerts.size(); j++)
                {
                    glm::vec3 n = centerOfMass - glm::vec3(fakeParticleModelVerts[j]);
                    fakeParticleModelVerts[j] = fakeParticleModelVerts[j] + glm::vec4(n * 0.5f, 0.f) - glm::vec4(offset, 0.f);
                }

                // auto gjkResult = naiveGjk(&otherCollider, &fakeParticleModel, particles);
                std::vector<glm::vec4> otherColliderVerts;
                for (int j = 0; j < otherCollider.mesh->vertices.size(); j++)
                {
                    // otherColliderVerts.push_back(glm::vec4(particles[otherCollider.mesh->offsetInParticles + j].position, 0.f));
                    otherColliderVerts.push_back(glm::vec4(particles[otherCollider.mesh->particleIndices[j]].position, 0.f));
                }
                
                // auto gjkResult = naiveGjk(&otherCollider, fakeParticleModelVerts, particles);
                auto gjkResult = naiveGjk(otherColliderVerts, fakeParticleModelVerts);
                if (gjkResult.collides)
                {
                    // auto col = epa(&otherCollider, &fakeParticleModel, gjkResult, particles);
                    auto col = epa(otherColliderVerts, fakeParticleModelVerts, gjkResult);
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
            
                    particle.position += col.collisionVec / 100.f * 0.45f * (isDynamic(particle) ? 1.f : 0.f);
                    std::println("{} {} {}", col.collisionVec.x, col.collisionVec.y, col.collisionVec.z);

                    // NOTE(savas): flag for colouring vertices in the shader
                    collider.mesh->vertices[i].w = 112233.f;
                    // fakeParticleModel.color = glm::vec3(1.f, 0.f, 1.f);
                }

                // collisionModels.push_back(fakeParticleModel);
            }
        };

    for (auto& collision : collisions)
    {
        if (collision.a != nullptr && collision.b != nullptr)
        {
            if (collision.a->mesh->vertices.size() <= 8 && collision.b->mesh->vertices.size() <= 8)
            {
                handler(*collision.a, *collision.b, glm::vec3(1.f, 1.f, 1.f));
                handler(*collision.b, *collision.a, glm::vec3(0.5f, 0.5f, 0.5f));
            }
            // if (collision.b->mesh->vertices.size() <= 8)
            // {
            //     handler(*collision.b, *collision.a, glm::vec3(0.5f, 0.5f, 0.5f));
            // }
        }
    }
}

void solveNewCollisionConstraints(float dt, std::vector<Particle>& particles, std::vector<Collision>& collisions, std::vector<Model>& collisionModels)
{
    /*
    const auto offsetParticles = [&particles](Model& collider, glm::vec3 collisionVec)
    {
        for (int i = 0; i < collider.mesh->indices.size(); i += 3)
        {
            uint32_t triangleIndices[] = 
            {
                collider.mesh->indices[i + 0],
                collider.mesh->indices[i + 1],
                collider.mesh->indices[i + 2],
            };
            glm::vec3 triangle[3] = 
            {
                collider.mesh->vertices[collider.mesh->indices[i + 0]],
                collider.mesh->vertices[collider.mesh->indices[i + 1]],
                collider.mesh->vertices[collider.mesh->indices[i + 2]],
            };
        
            glm::vec3 colLine[] = 
            {
                collisionVec * 0.9f,
                collisionVec * 1.1f,
            };
            auto [intersects, barycentric] = lineTriangleIntersection(colLine[0], colLine[1], triangle);
            if (!intersects)
            {
                continue;
            }

            for (int j = 0; j < 3; j++) 
            {
                // NOTE(savas): would be more correct if >= 0.5
                if (barycentric[j] >= 0.45)
                {
                    Particle& particle = particles[collider.mesh->particleIndices[triangleIndices[j]]];
                    particle.position += collisionVec * 0.5f; 
                }
            }
        }
    };
    
    for (auto& collision : collisions)
    {
        if (collision.a != nullptr && collision.b != nullptr)
        {
            offsetParticles(collision.a, collision.collisionVec);
            offsetParticles(collision.b, collision.collisionVec);
        }
    }
    
    // NOTE(savas): utterly horrible
    const auto handler = [&particles, &collisionModels](Model& collider, Model& otherCollider, glm::vec3 color, Collision& collision)
        {
            for (int i = 0; i < collider.mesh->indices.size(); i += 3)
            {
                uint32_t triangleIndices[] = 
                {
                    collider.mesh->indices[i + 0],
                    collider.mesh->indices[i + 1],
                    collider.mesh->indices[i + 2],
                };
                glm::vec3 triangle[3] = 
                {
                    collider.mesh->vertices[collider.mesh->indices[i + 0]],
                    collider.mesh->vertices[collider.mesh->indices[i + 1]],
                    collider.mesh->vertices[collider.mesh->indices[i + 2]],
                };
                
                glm::vec3 colLine[] = 
                {
                    collision.collisionVec * 0.9f,
                    collision.collisionVec * 1.1f,
                };
                auto [intersects, barycentric] = lineTriangleIntersection(colLine[0], colLine[1], triangle);
                if (!intersects)
                {
                    continue;
                }

                for (int j = 0; j < 3; j++) 
                {
                    // NOTE(savas): would be more correct if >= 0.5
                    if (barycentric[j] >= 0.45)
                    {
                        Particle& particle = particles[collider.mesh->particleIndices[triangleIndices[j]]];
                        particle.position += collision.collisionVec * 0.5f; 
                    }
                }
            }


            
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
    */  
}

void adjustVelocities(float dt, std::vector<Particle>& particles)
{
    for (auto& particle : particles)
    {
        if (isDynamic(particle))
        {
            particle.velocity = (particle.position - particle.previousPosition) / dt;
        }
    }
}
