#pragma once

#include "physics/xpbd.h"

#include <glm/glm.hpp>

#include <vector>
#include <span>

struct GjkInfo 
{
    bool collides;  
    std::vector<glm::vec3> simplex;
    glm::vec3 dir;
};
class Model;
GjkInfo naiveGjk(const std::span<glm::vec4>& a, const std::span<glm::vec4>& b);

Collision epa(const std::span<glm::vec4>& a, const std::span<glm::vec4>& b, GjkInfo info);

struct Constraints 
{
    
};
std::vector<Collision> detectCollisions(std::vector<Model>& models, std::vector<Particle>& particles);
