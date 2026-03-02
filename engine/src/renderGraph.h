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


template<typename T>
concept Resource = requires(T t, typename T::TransitionValue transitionValue)
{
    typename T::TransitionValue;

    { t.readTransition(transitionValue); } -> std::same_as<void>;
    { t.writeTransition(transitionValue); } -> std::same_as<void>;
};

template <typename T, typename... Args>
concept RenderPass = requires(T t, typename T::Resources resources, Args&&... args)
{
    typename T::Resources;

    { t.resources(std::forward<Args>(args)...); } -> std::same_as<typename T::Resources>;

    { t.hasSideEffects; } -> std::convertable_to<bool>;

    // TODO: temporary hack until I discover how to do this nicely and without dynamic dispatch
    { t.registerCallbacks(resources); } -> std::same_as<void>;
    // {
    //     t.prepare(
    //         std::declval<NewCompiledRenderGraph>(),
    //         std::declval<const RenderContext&>(),
    //         std::declval<typename T::Resource>
    //     );
    // } -> std::same_as<void>;

    // {
    //     t.render(
    //         std::declval<NewCompiledRenderGraph>(),
    //         std::declval<const RenderContext&>(),
    //         std::declval<typename T::Resource>
    //     );
    // } -> std::same_as<void>;
};


struct NewRenderGraph
{
    template<typename T, typename... Inputs, typename... Outputs>
        requires RenderPass<T, Inputs...>
    auto addPass(T&& pass, Inputs&&... inputs) -> auto
    {
        auto result = pass.resources(inputs);

        // TEMP: for compatibility
        pass.registerCallbacks(result);
        
        // TEMP: this is too for compatibility
        auto renderPass = createPass(*this);
        renderPass.pass.debugName = pass.name; 
        renderPass.pass.pipeline = pass.pipeline;
    }

    template<typename T>
        require Resource<T>
    auto importResource(T res) -> RenderGraphResource<T>
    {
        auto handle = getHandle(graph);
        graph.resources[handle] = RenderGraph::Resource {
            .data = data,
        };
        graph.layouts[handle] = layout;
        return handle;
    }

    template<typename T>
        requires Resource<T>
    auto readResource(RenderGraphResource<T> res, typename T::TransitionValue transitionValue) -> RenderGraphResource<T>
    {
        res.readTransition(u);
    }

    template<typename T>
        requires Resource<T>
    auto writeResource(RenderGraphResource<T> res, typename T::TransitionValue transitionValue) -> RenderGraphResource<T>
    {
        res.writeTransition(u);
    }

    auto compile() -> NewCompiledRenderGraph;
};

struct NewCompiledRenderGraph
{
    template<typename T>
        requires Resource<T>
    auto getResource(RenderGraphResource<T> res) -> T*
    {
        // TODO: would be nice to implement some sort of ensurance that this casting is valid
        return static_cast<T*>(graph.resources[handle]);
    }

    auto execute(const RenderContext& ctx)
    {
        for (auto& pass : passes)
        {
            pass.prepare(*this, ctx, pass.resources)
            pass.render(*this, ctx, pass.resources)
        }
    }
};

struct SDFPass
{
    struct GBuffer
    {
        RenderGraphResource<BindlessTexture> color;  
    };

    using Resources = GBuffer;
    
    auto resources(RenderGraph rg, ResourceHandle<BindlessTexture> color) -> GBuffer
    {
        return
        {
            .color = rg.writeResource(color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        };
    }

    auto prepare(CompiledRenderGraph& rg, const RenderContext& ctx, GBuffer resources) -> void
    {
        
    }

    auto render(CompiledRenderGraph rg, const RenderContext& ctx, GBuffer resources) -> void
    {
        
    }
};


void renderer()
{
    RenderGraph rg {
        .allowResourceAliasing = true
    };

    const auto [draws, lights, instanceData] = rg.addPass(SceneDataUploader{});
    const auto depth = rg.addPass(ZPrePass{}, draws);
    const auto shadows = rg.addPass(CSMPass{}, draws);
    const auto lightData = rg.addPass(TiledLightCullingPass{}, lights, depth);
    auto [color, normal] = rg.addPass(OpaqueForwardPass{}, draws, depth, shadows, lightData);
    std::tie(color, normal) = rg.addPass(SDFGeometryPass{}, color, depth);

    auto output = color;

    // Post processing
    output = rg.addPass(AtmospherePass{}, output, depth);
    output = rg.addPass(BloomPass{}, output);
    output = rg.addPass(ReinhardTonemapPass{}, output);
    output = rg.addPass(SMAAPass{}, output);

    output = rg.addPass(ImguiPass{}, swapchainRes);
    rg.addPass(BlitPass{}, output, swapchainRes);

    // if (rg.hash() != compiledRenderGraph.hash())
    // {
    //     compiledRenderGraph = rg.compile();
    // }
    compiledRenderGraph = rg.compile();
}














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
    const bool allowResourceAliasing;

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
