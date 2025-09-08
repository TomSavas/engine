#pragma once

#include "engine.h"

#include <glm/glm.hpp>

#include <vector>

struct Instance;

struct SceneGraph
{
    struct Node
    {
        std::string name;

        glm::mat4 localTransform;
        glm::mat4 globalTransform;

        u16 predecessorCount;

        Node* parent;
        std::vector<Node*> children;

        Instance* instance;

        //bool dirtyTransform;
    };

    //bool hierarchyDirty;
    //std::vector<Node> preOrderNodes;

    Node* root;
};

auto updateSceneGraphTransforms(SceneGraph& sceneGraph) -> void;
