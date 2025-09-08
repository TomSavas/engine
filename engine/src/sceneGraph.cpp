#include "sceneGraph.h"

auto updateTransform(SceneGraph::Node& node, glm::mat4 parentTransform) -> void
{
    node.globalTransform = parentTransform * node.localTransform;
    for (auto& child : node.children)
    {
        updateTransform(*child, node.globalTransform);
    }
}

auto updateSceneGraphTransforms(SceneGraph& sceneGraph) -> void
{
    //// TODO: could do this only on dirty nodes

    //// Simply to ensure that the parent transforms are always calculated
    //if (sceneGraph.hierarchyDirty)
    //{
    //    // Ascending order of how many predecessors the node has
    //    std::sort(sceneGraph.nodes.begin(), sceneGraph.nodes.end(),
    //        [](const SceneGraph::Node& n0, const SceneGraph::Node& n1)
    //        {
    //            return n0.predecessorCount < n1.predecessorCount;;
    //        });
    //}

    //// Recalculate transforms
    //for (auto& node : sceneGraph.nodes)
    //{
    //    const auto parentTransform = node.parent == nullptr ? glm::mat4(1.f) : node.parent->globalTransform;
    //    node.globalTransform = parentTransform * node.localTransform;
    //}

    for (auto& child : sceneGraph.root->children)
    {
        updateTransform(*child, glm::mat4(1.f));
    }
}