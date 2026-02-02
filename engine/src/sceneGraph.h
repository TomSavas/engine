#pragma once

#include "engine.h"
#include "mesh.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>
#include <optional>

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

        // Debug
        u32 materialIndex = -1;

        //bool dirtyTransform;
    };
    Node* root;

    using NodeHandle = u32;
    static constexpr NodeHandle kRootHandle = 0;
    static constexpr NodeHandle kInvalidHandle = ~0;
    
    struct NewNode
    {
        enum class TransformDirtiness : u8
        {
            LOCAL_DIRTY = 0,
            GLOBAL_DIRTY = 1,
            CLEAN
        };
        
        std::string name;

        NodeHandle parent;
        TransformDirtiness dirty;

        glm::mat4 localTransform;
        glm::mat4 globalTransform;
        
        std::optional<ModelHandle> model;
        std::optional<InstanceHandle> instance;
    };

    //bool hierarchyDirty;
    //std::vector<Node> preOrderNodes;

    std::vector<NewNode> nodes;

    auto addChildNodes(std::vector<NodeHandle>& parents, std::vector<glm::mat4>& transforms, std::string_view baseName) -> std::vector<NodeHandle>;
    auto addEmptyChildNodes(std::vector<NodeHandle>& parents) -> std::vector<NodeHandle>;
};

auto updateSceneGraphTransforms(SceneGraph& sceneGraph) -> void;
