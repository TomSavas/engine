#include <vulkan/vulkan_core.h>
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
    Handle handleCount {0};
};

Handle getHandle(RenderGraph& graph)
{
    const Handle handle = graph.handleCount;
    graph.handleCount++;
    return handle;
}

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
};

template<typename T>
struct RenderGraphResource
{
    using Data = T;
    Handle handle;
    Data* data;
};
using RenderGraphBufferResource = RenderGraphResource<VkBuffer>;

// TODO: implement
template<typename T>
T createResource(RenderGraph& graph, typename T::Data* data) { static_assert(false); return T{}; }

template<typename T>
T importResource(RenderGraph& graph, typename T::Data* data)
{
    return T {
        .handle = getHandle(graph),
        .data = data
    };
}

template<typename T>
typename T::Data& getResource(CompiledRenderGraph& graph, RenderGraphResource<T> resource)
{
    
}

void registerPass(RenderGraph& graph, RenderPass pass);
CompilerRenderGraph compile(RenderGraph&& graph);
