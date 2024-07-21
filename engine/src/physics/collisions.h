#pragma once

#include <glm/glm.hpp>

#include <vector>

struct GjkInfo 
{
    bool collides;  
    std::vector<glm::vec3> simplex;
    glm::vec3 dir;
};
class Model;
GjkInfo naiveGjk(Model* a, Model* b);

struct Collision 
{
    Model* a;
    Model* b;
    glm::vec3 collisionVec;
};
Collision epa(Model* a, Model* b, GjkInfo info);

struct Constraints 
{
    
};
std::vector<Constraints> generateXpbdCollisionConstraints(std::vector<Collision> collision);

std::vector<Collision> detectCollisions(std::vector<Model>& models);
