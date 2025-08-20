#include "engine.h"

#include "renderGraph.h"

#include <limits>

auto getHandle(RenderGraph& graph) -> Handle
{
    const auto handle = graph.resources.size();
    graph.resources.push_back(nullptr);
    graph.layouts.push_back(VK_IMAGE_LAYOUT_UNDEFINED);
    return handle;
}

//auto createPass(RenderGraph& graph, std::string&& debugName) -> RenderGraph::Node&
auto createPass(RenderGraph& graph) -> RenderGraph::Node&
{
    // FIXME: stupid and unsafe
    auto& node = graph.nodes.emplace_back();
    //node.pass.debugName = debugName;
    return node;
}

auto compile(VulkanBackend& backend, RenderGraph&& graph) -> CompiledRenderGraph
{
    CompiledRenderGraph compiledGraph;

    for (auto& node : graph.nodes)
    {
        auto& compiledNode = compiledGraph.nodes.emplace_back();
        compiledNode.pass = node.pass;

        u32 read = 0;
        u32 write = 0;
        while (read + write < node.reads.size() + node.writes.size())
        {
            const auto newReadHandle = read < node.reads.size() ? node.reads[read].newHandle : std::numeric_limits<u32>::max();
            const auto newWriteHandle = write < node.writes.size() ? node.writes[write].newHandle : std::numeric_limits<u32>::max();
            const auto& olderAccess = newReadHandle < newWriteHandle ? node.reads[read] : node.writes[write];
            (newReadHandle < newWriteHandle ? read : write) += 1;

            olderAccess.transition(backend, compiledNode, graph.resources[olderAccess.newHandle],
                graph.layouts[olderAccess.oldHandle], graph.layouts[olderAccess.newHandle]);
        }
    }
    compiledGraph.resources = graph.resources;

    return compiledGraph;
}
