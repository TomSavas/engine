#pragma once

#include "engine.h"
#include "rhi/renderpass.h"

#include <vulkan/vulkan_core.h>

#include <functional>
#include <limits>
#include <vector>

class VulkanBackend;

using Handle = u32;
// TODO: Make this non VK specific
using Layout = VkImageLayout;

constexpr Handle kInvalidHandle = std::numeric_limits<Handle>::max();

// TODO: Make these non VK specific
using ImageBarrier = VkImageMemoryBarrier2;
using BufferBarrier = VkBufferMemoryBarrier2;
using MemoryBarrier = VkMemoryBarrier2;

struct CompiledRenderGraph
{
    struct Node
    {
        std::vector<ImageBarrier> imageBarriers;
        std::vector<BufferBarrier> bufferBarriers;
        std::vector<MemoryBarrier> memoryBarriers;
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
        // Handle oldHandle;
        // Handle newHandle;
        Handle handle;
        std::function<void(VulkanBackend& backend, CompiledRenderGraph::Node&, void*, Layout, Layout)> transition;
    };
    struct Node
    {
        std::vector<ResourceAccess> reads;
        std::vector<ResourceAccess> writes;
        RenderPass pass;
        u32 selfHandle;
    };
    struct Resource
    {
        Handle prevVersion = kInvalidHandle;
        Handle nextVersion = kInvalidHandle;
        // u32 ownerNode = std::numeric_limits<Handle>::max();
        void* data;
    };

    std::vector<Node> nodes;
    // TODO: change void* to std::variant or better yet -- concepts
    // std::vector<void*> resources;
    std::vector<Resource> resources;
    std::vector<Layout> layouts;
};

auto getHandle(RenderGraph& graph) -> Handle;

// TODO: implement
//template<typename T>
//[[nodiscard]]
//auto createResource(RenderGraph& graph, RenderGraph::Node& node, typename T::Data* data) -> T
//{
//    static_assert(false);
//    return T{};
//}

template <typename T>
using RenderGraphResource = Handle;

template <typename T>
[[nodiscard]]
auto importResource(RenderGraph& graph, RenderGraph::Node& node, T* data, Layout layout = VK_IMAGE_LAYOUT_UNDEFINED)
    -> RenderGraphResource<T>
{
    auto handle = getHandle(graph);
    graph.resources[handle] = RenderGraph::Resource {
        .data = data,
    };
    graph.layouts[handle] = layout;
    return handle;
}

template <typename T>
auto readResource(RenderGraph& graph, RenderGraph::Node& node, RenderGraphResource<T> handle,
    Layout layout = VK_IMAGE_LAYOUT_UNDEFINED)
    -> RenderGraphResource<T>
{
    // Find latest handle
    while (graph.resources[handle].nextVersion != kInvalidHandle)
    {
        handle = graph.resources[handle].nextVersion;
    }

    auto newHandle = getHandle(graph);
    graph.resources[handle].nextVersion = newHandle;
    graph.resources[newHandle] = RenderGraph::Resource {
        .prevVersion = handle,
        .data = graph.resources[handle].data,
        // .ownerNode = node.selfHandle,
    };
    graph.layouts[newHandle] = layout;

    node.reads.push_back({
        // .oldHandle = handle,
        // .newHandle = newHandle,
        .handle = newHandle,
        .transition = [](VulkanBackend& backend, CompiledRenderGraph::Node& compiledNode, void* data, Layout oldLayout,
            Layout newLayout)
        {
            addTransition<T>(backend, compiledNode, static_cast<T*>(data), oldLayout, newLayout);
        }
    });

    return newHandle;
}

template <typename T>
auto writeResource(RenderGraph& graph, RenderGraph::Node& node, RenderGraphResource<T> handle,
    Layout layout = VK_IMAGE_LAYOUT_UNDEFINED)
    -> RenderGraphResource<T>
{
    // TODO: this WILL cause a bug. At this point we should be allocating a new resource

    // Find latest handle
    while (graph.resources[handle].nextVersion != kInvalidHandle)
    {
        handle = graph.resources[handle].nextVersion;
    }

    auto newHandle = getHandle(graph);
    // Don't set next version, as this should be treated as a completely new resource.
    // BUG: WE SHOULD NOT BE SETTING THIS
    graph.resources[handle].nextVersion = newHandle;
    graph.resources[newHandle] = RenderGraph::Resource {
        .prevVersion = handle,
        .data = graph.resources[handle].data,
        // .ownerNode = node.selfHandle,
    };
    graph.layouts[newHandle] = layout;

    node.writes.push_back({
        .handle = newHandle,
        // .oldHandle = handle,
        // .newHandle = newHandle,
        .transition = [](VulkanBackend& backend, CompiledRenderGraph::Node& compiledNode, void* data, Layout oldLayout,
            Layout newLayout)
        {
            addTransition<T>(backend, compiledNode, static_cast<T*>(data), oldLayout, newLayout);
        }
    });

    return newHandle;
}

template <typename T>
auto addTransition(VulkanBackend& backend, CompiledRenderGraph::Node& compiledNode, T* resource, Layout oldLayout,
    Layout newLayout)
    -> void
{
}

template <typename T>
[[nodiscard]]
auto getResource(CompiledRenderGraph& graph, RenderGraphResource<T> handle) -> T*
{
    // TODO: would be nice to implement some sort of ensurance that this casting is valid
    return static_cast<T*>(graph.resources[handle]);
}

[[nodiscard]]
auto createPass(RenderGraph& graph) -> RenderGraph::Node&;
[[nodiscard]]
auto compile(VulkanBackend& backend, RenderGraph&& graph) -> CompiledRenderGraph;
