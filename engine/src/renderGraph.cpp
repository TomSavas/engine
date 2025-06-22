#include "renderGraph.h"

Handle getHandle(RenderGraph& graph)
{
    const Handle handle = graph.resources.size();
    graph.resources.push_back(nullptr);
    return handle;
}

RenderGraph::Node& createPass(RenderGraph& graph)
{
    // FIXME: stupid and unsafe
    RenderGraph::Node& node = graph.nodes.emplace_back();
    return node;
}

CompiledRenderGraph compile(RenderGraph&& graph)
{
    // NOTE: this is a stub implementation for the time being
    CompiledRenderGraph compiledGraph;

    for (RenderGraph::Node& node : graph.nodes)
    {
        compiledGraph.nodes.push_back(CompiledRenderGraph::Node{.pass = node.pass});
    }
    compiledGraph.resources = graph.resources;

    return compiledGraph;
}
