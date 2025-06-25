#include "renderGraph.h"

#include <limits>

#include "rhi/vulkan/utils/image.h"
#include "rhi/vulkan/utils/inits.h"

Handle getHandle(RenderGraph& graph)
{
    const Handle handle = graph.resources.size();
    graph.resources.push_back(nullptr);
    graph.layouts.push_back(VK_IMAGE_LAYOUT_UNDEFINED);
    return handle;
}

RenderGraph::Node& createPass(RenderGraph& graph)
{
    // FIXME: stupid and unsafe
    RenderGraph::Node& node = graph.nodes.emplace_back();
    return node;
}

CompiledRenderGraph compile(VulkanBackend& backend, RenderGraph&& graph)
{
    CompiledRenderGraph compiledGraph;

    for (RenderGraph::Node& node : graph.nodes)
    {
        CompiledRenderGraph::Node& compiledNode = compiledGraph.nodes.emplace_back();
        compiledNode.pass = node.pass;

        int read = 0;
        int write = 0;
        while (read + write < node.reads.size() + node.writes.size())
        {
            const Handle newReadHandle = read < node.reads.size() ? node.reads[read].newHandle : std::numeric_limits<int>::max();
            const Handle newWriteHandle = write < node.writes.size() ? node.writes[write].newHandle : std::numeric_limits<int>::max();
            const RenderGraph::ResourceAccess& olderAccess = newReadHandle < newWriteHandle ? node.reads[read] : node.writes[write];
            (newReadHandle < newWriteHandle ? read : write) += 1;

            olderAccess.transition(backend, compiledNode, graph.resources[olderAccess.newHandle],
                graph.layouts[olderAccess.oldHandle], graph.layouts[olderAccess.newHandle]);
        }
    }
    compiledGraph.resources = graph.resources;

    return compiledGraph;
}
