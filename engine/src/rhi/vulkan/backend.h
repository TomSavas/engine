#pragma once

#include "engine.h"
#include "renderGraph.h"
#include "rhi/renderpass.h"
#include "rhi/vulkan/bindless.h"
#include "rhi/vulkan/descriptors.h"
#include "rhi/vulkan/shader.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/image.h"
#include "rhi/vulkan/utils/texture.h"

#include "VkBootstrap.h"
#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"
#include "vk_mem_alloc.h"
#include "result.hpp"

#include <chrono>
#include <functional>


struct Stats
{
    u64 finishedFrameCount = 0;
};

struct FrameCtx
{
    VkSemaphore presentSem;
    VkSemaphore renderSem;
    VkFence renderFence;

    VkFence tracyRenderFence;
    VkCommandPool tracyCmdPool;
    VkCommandBuffer tracyCmdBuffer;
    TracyVkCtx tracyCtx;

    VkCommandPool cmdPool;
    VkCommandBuffer cmdBuffer;

    VkCommandPool cmdComputePool;
    VkCommandBuffer cmdComputeBuffer;
};

class GLFWwindow;
struct Scene;
struct Mesh;
struct CompiledRenderGraph;

class VulkanBackend;
enum class backendError {};
result::result<VulkanBackend*, backendError> initVulkanBackend();

struct FrameStats
{
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    u64 frameIndex;
    bool shutdownRequested;
    f64 pastFrameDt;
};

struct Frame
{
    FrameStats stats;
    std::reference_wrapper<FrameCtx> ctx;
};

struct CurrentSwapchain
{
    VkExtent2D size;
    VkFormat format;
    VkImage image;
    VkImageView view;
};

struct RenderContext
{
    // Frame& frame;
    CompiledRenderGraph& graph;
    VkCommandBuffer& cmd;
    Scene& scene;

    CurrentSwapchain swapchain;
};

struct VulkanBackend
{
    GLFWwindow* window;

    vkb::Instance vkbInstance;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkSurfaceKHR surface;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkPhysicalDeviceProperties gpuProperties;

    VkQueue graphicsQueue;
    u32 graphicsQueueFamily;

    VkQueue computeQueue;
    u32 computeQueueFamily;

    VkViewport viewport;
    VkRect2D scissor;

    // Swapchain
    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    // Immediate ctx
    VkFence immediateFence;
    VkCommandPool immediateCmdPool;
    VkCommandBuffer immediateCmdBuffer;

    // Backbuffer
    AllocatedImage backbufferImage;

    // Frames
    static constexpr i32 MaxFramesInFlight = 1;
    FrameCtx frames[MaxFramesInFlight];
    u64 currentFrameNumber = 0;

    // Allocators
    VmaAllocator allocator;

    DescriptorAllocator descriptorAllocator;

    // Caches
    ShaderModuleCache shaderModuleCache;

    // Profiler
    TracyVkCtx tracyCtx;

    // Debug
    Stats stats;

    // Resources
    std::unordered_map<std::string, Texture> textureCache;
    std::optional<Textures> textures;
    std::optional<BindlessResources> bindlessResources;

    VkDescriptorSetLayout sceneDescriptorSetLayout;

    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
    PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;

    explicit VulkanBackend() {}
    explicit VulkanBackend(GLFWwindow* window);
    // TODO: init?
    auto deinit() -> void;

    auto currentFrame() -> FrameCtx&;
    auto newFrame() -> Frame;
    auto endFrame(Frame&& frame) -> FrameStats;

    auto render(const Frame& frame, CompiledRenderGraph& compiledRenderGraph, Scene& scene, RenderGraphResource<BindlessTexture> output) -> void;

    auto addOutputBlitPass(RenderGraph& graph, RenderGraphResource<BindlessTexture> output) -> void;
    auto addImguiPass(RenderGraph& graph) -> void;

    auto immediateSubmit(std::function<void(VkCommandBuffer)>&& f) -> void;
    auto copyBuffer(std::optional<VkCommandBuffer> cmd, VkBuffer src, VkBuffer dst, VkBufferCopy copyRegion) -> void;
    auto copyBufferWithStaging(std::optional<VkCommandBuffer> cmd, void* data, size_t size, VkBuffer dst,
        VkBufferCopy copyRegion = VkBufferCopy())
        -> void;

    auto allocateBuffer(VkBufferCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags,
        VkMemoryPropertyFlags requiredFlags) -> AllocatedBuffer;
    // auto allocateImage(VkImageCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags,
    //     VkMemoryPropertyFlags requiredFlags, VkImageAspectFlags aspectFlags, const std::string& debugName = "") -> AllocatedImage;

    auto allocateImage(VkImageCreateInfo imageInfo, VmaAllocationCreateInfo allocInfo, MipOptions mipOpts,
        VkImageAspectFlagBits aspectFlags) -> AllocatedImage;
    auto allocateTexture(const std::string& name, VkImageCreateInfo imageInfo,
        VmaAllocationCreateInfo allocInfo, MipOptions mipOpts, VkImageAspectFlagBits aspectFlags) -> Texture;
    auto createTexture(const std::string& name, RawTexture rawTexture, VkImageCreateInfo imageInfo,
        VmaAllocationCreateInfo allocInfo, MipOptions mipOpts, VkImageAspectFlagBits aspectFlags)
        -> Texture;

    auto getBufferDeviceAddress(VkBuffer buffer) -> VkDeviceAddress;

private:
    auto initVulkan() -> void;
    auto initSwapchain() -> void;
    auto initCommandBuffers() -> void;
    auto initSyncStructs() -> void;
    auto initDescriptors() -> void;
    auto initImgui() -> void;
    auto initProfiler() -> void;
};
