#include "sceneGraph.h"

#include <numeric>
#include <stack>
#include <print>

// auto updateTransform(SceneGraph::Node& node, glm::mat4 parentTransform) -> void
// {
//     node.globalTransform = parentTransform * node.localTransform;
//     for (auto& child : node.children)
//     {
//         updateTransform(*child, node.globalTransform);
//     }
// }

// auto updateSceneGraphTransforms(SceneGraph& sceneGraph) -> void
// {
//     //// TODO: could do this only on dirty nodes

//     //// Simply to ensure that the parent transforms are always calculated
//     //if (sceneGraph.hierarchyDirty)
//     //{
//     //    // Ascending order of how many predecessors the node has
//     //    std::sort(sceneGraph.nodes.begin(), sceneGraph.nodes.end(),
//     //        [](const SceneGraph::Node& n0, const SceneGraph::Node& n1)
//     //        {
//     //            return n0.predecessorCount < n1.predecessorCount;;
//     //        });
//     //}

//     //// Recalculate transforms
//     //for (auto& node : sceneGraph.nodes)
//     //{
//     //    const auto parentTransform = node.parent == nullptr ? glm::mat4(1.f) : node.parent->globalTransform;
//     //    node.globalTransform = parentTransform * node.localTransform;
//     //}

//     for (auto& child : sceneGraph.root->children)
//     {
//         updateTransform(*child, glm::mat4(1.f));
//     }
// }
static i32 genNodeNameCount = 0;

auto SceneGraph::addChildNodes(std::vector<NodeHandle>& parents, std::vector<glm::mat4>& transforms, std::string_view baseName) -> std::vector<NodeHandle>
{
    if (nodes.size() == 0)
    {
        nodes.push_back(NewNode{
            .name = "root",
            .parent = kInvalidHandle,
            .dirty = NewNode::TransformDirtiness::CLEAN,
            .localTransform = glm::mat4(1.f),
            .globalTransform = glm::mat4(1.f),
            .model = std::nullopt,
            .instance = std::nullopt
        });
    }

    std::vector<NodeHandle> newNodes(parents.size());
    std::iota(newNodes.begin(), newNodes.end(), static_cast<NodeHandle>(nodes.size()));

    i32 nodeNameCount = 0;

    // TODO: should be a touch smarter
    nodes.resize(nodes.size() + parents.size());
    for (size_t i{}; i < parents.size(); ++i)
    {
        // nodes[newNodes[i]].parent = parents[i];

        nodes[newNodes[i]] = NewNode{
            .name = baseName.empty()
                ? std::format("genNode{}", genNodeNameCount++)
                : std::format("{}{}", baseName, nodeNameCount++),
            .parent = parents[i],
            // .dirty = NewNode::TransformDirtiness::LOCAL_DIRTY,
            .dirty = NewNode::TransformDirtiness::GLOBAL_DIRTY,
            // .localTransform = glm::mat4(1.f),
            .localTransform = transforms[i],
            .globalTransform = glm::mat4(1.f),
            // .globalTransform = transforms[i],
            .model = std::nullopt,
            .instance = std::nullopt
        };
    }

    return newNodes;
}

auto SceneGraph::addEmptyChildNodes(std::vector<NodeHandle>& parents) -> std::vector<NodeHandle>
{
    if (nodes.size() == 0)
    {
        nodes.push_back(NewNode{
            .name = "root",
            .parent = kInvalidHandle,
            .dirty = NewNode::TransformDirtiness::CLEAN,
            .localTransform = glm::mat4(1.f),
            .globalTransform = glm::mat4(1.f),
            .model = std::nullopt,
            .instance = std::nullopt
        });
    }

    std::vector<NodeHandle> newNodes(parents.size());
    std::iota(newNodes.begin(), newNodes.end(), static_cast<NodeHandle>(nodes.size()));

    // TODO: should be a touch smarter
    nodes.resize(nodes.size() + parents.size());
    for (size_t i{}; i < parents.size(); ++i)
    {
        // nodes[newNodes[i]].parent = parents[i];
        nodes[newNodes[i]] = NewNode{
            .name = std::format("genNode{}", genNodeNameCount++),
            .parent = parents[i],
            .dirty = NewNode::TransformDirtiness::GLOBAL_DIRTY,
            .localTransform = glm::mat4(1.f),
            .globalTransform = glm::mat4(1.f),
            .model = std::nullopt,
            .instance = std::nullopt
        };
    }

    return newNodes;
}

auto updateSceneGraphTransforms(SceneGraph& sceneGraph) -> void
{
    // sceneGraph.nodes[SceneGraph::kRootHandle].dirty = SceneGraph::NewNode::TransformDirtiness::CLEAN;
    // sceneGraph.nodes[SceneGraph::kRootHandle].globalTransform = sceneGraph.nodes[SceneGraph::kRootHandle].localTransform;
    for (size_t i{}; i < sceneGraph.nodes.size(); ++i)
    {

        // TODO: array
        std::stack<size_t> ancestors;
        ancestors.push(i);
        // size_t latestDirtyAncestorIndex = 0;
        size_t dirtyCount = sceneGraph.nodes[i].dirty != SceneGraph::NewNode::TransformDirtiness::CLEAN ? 1 : 0;
        while(ancestors.top() != SceneGraph::kRootHandle)
        {
            auto& node = sceneGraph.nodes[ancestors.top()];
            if (node.parent != SceneGraph::kInvalidHandle)
            {
                ancestors.push(node.parent);
                
                auto& parentNode = sceneGraph.nodes[node.parent];
                if (parentNode.dirty != SceneGraph::NewNode::TransformDirtiness::CLEAN)
                {
                    // latestDirtyAncestorIndex = ancestors.size() - 1;
                    dirtyCount = ancestors.size();
                }
            }
        }

        // TODO: for this to work correctly we need to maintain a list of nodes we "cleaned" up.
        // Currently we're recalculating everything in every pass
        glm::mat4 globalTransform(1.f);
        while(ancestors.size() > dirtyCount)        
        {
            globalTransform = sceneGraph.nodes[ancestors.top()].globalTransform;
            ancestors.pop();
        }

        while(!ancestors.empty())
        {
            auto index = ancestors.top();
            ancestors.pop();
            auto& node = sceneGraph.nodes[index];

            switch (node.dirty)
            {
            case SceneGraph::NewNode::TransformDirtiness::LOCAL_DIRTY:
                node.localTransform = glm::inverse(globalTransform) * node.globalTransform;
                break;
            case SceneGraph::NewNode::TransformDirtiness::CLEAN:
                // If we're clean but this got triggered, it means that one of our ancestors changed,
                // so just update the global transform
                [[fallthrough]];
            case SceneGraph::NewNode::TransformDirtiness::GLOBAL_DIRTY:
                [[fallthrough]];
            default:
                node.globalTransform = globalTransform * node.localTransform;
                break;
            }

            globalTransform = node.globalTransform;
        }
    }

    for (size_t i{}; i < sceneGraph.nodes.size(); ++i)
    {
        sceneGraph.nodes[i].dirty = SceneGraph::NewNode::TransformDirtiness::CLEAN;
    }

    {
        // std::println("[end][update] global transform: {} {} {}", sceneGraph.nodes[2].globalTransform[3][0],  sceneGraph.nodes[2].globalTransform[3][1], sceneGraph.nodes[2].globalTransform[3][2]);
        // std::println("[end][update] local transform: {} {} {}",  sceneGraph.nodes[2].localTransform[3][0],  sceneGraph.nodes[2].localTransform[3][1],  sceneGraph.nodes[2].localTransform[3][2]);
    }
}
