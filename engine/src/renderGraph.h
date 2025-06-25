#pragma once

#include <vulkan/vulkan_core.h>

#include "rhi/vulkan/renderpass.h"

#include <vector>
#include <functional>

class VulkanBackend;

using Handle = uint32_t;
using Layout = VkImageLayout;

struct CompiledRenderGraph
{
    struct Node
    {
        // Make these non VK specific
        std::vector<VkImageMemoryBarrier2> imageBarriers;
        std::vector<VkBufferMemoryBarrier2> bufferBarriers;
        std::vector<VkMemoryBarrier2> memoryBarriers;
        RenderPass pass;
    };

    std::vector<Node> nodes;
    std::vector<void*> resources;
};

struct RenderGraph
{
    VulkanBackend& backend;

    struct ResourceAccess
    {
        Handle oldHandle;
        Handle newHandle;
        std::function<void(VulkanBackend& backend, CompiledRenderGraph::Node&, void*, Layout, Layout)> transition;
    };
    struct Node
    {
        std::vector<ResourceAccess> reads;
        std::vector<ResourceAccess> writes;
        RenderPass pass;
    };

    std::vector<Node> nodes;
    // TODO: change void* to std::variant or better yet -- concepts
    std::vector<void*> resources;
    std::vector<Layout> layouts;
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
RenderGraphResource<T> importResource(RenderGraph& graph, RenderGraph::Node& node, T* data,
    Layout layout = VK_IMAGE_LAYOUT_UNDEFINED)
{
    Handle handle = getHandle(graph);
    graph.resources[handle] = data;
    graph.layouts[handle] = layout;
    return handle;
}

template <typename T>
RenderGraphResource<T> readResource(RenderGraph& graph, RenderGraph::Node& node, RenderGraphResource<T> handle,
    Layout layout = VK_IMAGE_LAYOUT_UNDEFINED)
{
    Handle newHandle = getHandle(graph);
    graph.resources[newHandle] = graph.resources[handle];
    graph.layouts[newHandle] = layout;

    node.reads.push_back({
        .oldHandle = handle,
        .newHandle = newHandle,
        .transition = [](VulkanBackend& backend, CompiledRenderGraph::Node& compiledNode, void* data, Layout oldLayout,
            Layout newLayout)
        {
            addTransition<T>(backend, compiledNode, static_cast<T*>(data), oldLayout, newLayout);
        }
    });

    return newHandle;
}

template <typename T>
RenderGraphResource<T> writeResource(RenderGraph& graph, RenderGraph::Node& node, RenderGraphResource<T> handle,
    Layout layout = VK_IMAGE_LAYOUT_UNDEFINED)
{
    Handle newHandle = getHandle(graph);
    graph.resources[newHandle] = graph.resources[handle];
    graph.layouts[newHandle] = layout;

    node.writes.push_back({
        .oldHandle = handle,
        .newHandle = newHandle,
        .transition = [](VulkanBackend& backend, CompiledRenderGraph::Node& compiledNode, void* data, Layout oldLayout,
            Layout newLayout)
        {
            addTransition<T>(backend, compiledNode, static_cast<T*>(data), oldLayout, newLayout);
        }
    });

    return newHandle;
}

template <typename T>
void addTransition(VulkanBackend& backend, CompiledRenderGraph::Node& compiledNode, T* resource, Layout oldLayout,
    Layout newLayout)
{
}

template <typename T>
T* getResource(CompiledRenderGraph& graph, RenderGraphResource<T> handle)
{
    // TODO: would be nice to implement some sort of ensurance that this casting is valid
    return static_cast<T*>(graph.resources[handle]);
}

RenderGraph::Node& createPass(RenderGraph& graph);
CompiledRenderGraph compile(VulkanBackend& backend, RenderGraph&& graph);