#pragma once

#include <vulkan/vulkan_core.h>

#include <vector>

#include "rhi/vulkan/renderpass.h"

using Handle = uint32_t;

struct RenderGraph
{
    struct Node
    {
        std::vector<Handle> reads;
        std::vector<Handle> writes;
        RenderPass pass;
    };

    std::vector<Node> nodes;
    // TODO: change void* to std::variant or better yet -- concepts
    std::vector<void*> resources;
};

struct CompiledRenderGraph
{
    struct Node
    {
        // Make these non VK specific
        std::vector<VkImageMemoryBarrier> imageBarriers;
        std::vector<VkBufferMemoryBarrier> bufferBarriers;
        std::vector<VkMemoryBarrier> memoryBarriers;
        RenderPass pass;
    };

    std::vector<Node> nodes;
    std::vector<void*> resources;
};

Handle getHandle(RenderGraph& graph);

// TODO: implement
// template<typename T>
// T createResource(RenderGraph& graph, RenderGraph::Node& node, typename T::Data* data)
// {
//     static_assert(false);
//     return T{};
// }

template <typename T>
using RenderGraphResource = Handle;

template <typename T>
RenderGraphResource<T> importResource(RenderGraph& graph, RenderGraph::Node& node, T* data)
{
    Handle handle = getHandle(graph);
    graph.resources[handle] = (void*)data;
    return handle;
}

template <typename T>
RenderGraphResource<T> readResource(RenderGraph& graph, RenderGraph::Node& node, RenderGraphResource<T> handle)
{
    node.reads.push_back(handle);
    Handle newHandle = getHandle(graph);
    graph.resources[newHandle] = graph.resources[handle];
    return newHandle;
}

template <typename T>
RenderGraphResource<T> writeResource(RenderGraph& graph, RenderGraph::Node& node, RenderGraphResource<T> handle)
{
    node.writes.push_back(handle);
    Handle newHandle = getHandle(graph);
    graph.resources[newHandle] = graph.resources[handle];
    return newHandle;
}

template <typename T>
T* getResource(CompiledRenderGraph& graph, RenderGraphResource<T> handle)
{
    // Inject barrier here
    return static_cast<T*>(graph.resources[handle]);
}

// template<typename T>
// struct RenderGraphResourceHandle
// {
//     using Data = T;
//     Handle handle;

//     Data* resolve(void* data)
// };

// template<typename T>
// RenderGraphResourceHandle<T> importResource(RenderGraph& graph, RenderGraph::Node& node, typename T::Data* data)
// {
//     RenderGraphResourceHandle<T> handle = getHandle(graph);
//     graph.resources[handle] = (void*)data;
//     return handle;
// }

// template<typename T>
// RenderGraphResourceHandle<T> readResource(RenderGraph& graph, RenderGraph::Node& node, Handle handle)
// {
//     node.reads.push_back(handle);
//     return getHandle(graph);
// }

// template<typename T>
// RenderGraphResourceHandle<T> writeResource(RenderGraph& graph, RenderGraph::Node& node, Handle handle)
// {
//     node.writes.push_back(handle);
//     return getHandle(graph);
// }

// template<typename T>
// typename T::Data* getResource(CompiledRenderGraph& graph, RenderGraphResourceHandle<T> handle)
// {
//     // Inject barrier here
//     return handle.resolve(graph.resources[handle])
//     return reinterpret_cast<T::Data>(graph.resources[handle]);
// }

RenderGraph::Node& createPass(RenderGraph& graph);
CompiledRenderGraph compile(RenderGraph&& graph);