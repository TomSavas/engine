#pragma once

#include <chrono>
#include <functional>

#include "VkBootstrap.h"
#include "engine.h"
#include "result.hpp"
#include "rhi/renderpass.h"
#include "rhi/vulkan/bindless.h"
#include "rhi/vulkan/descriptors.h"
#include "rhi/vulkan/shader.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/image.h"
#include "rhi/vulkan/utils/texture.h"
#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"
#include "vk_mem_alloc.h"

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
    static constexpr i32 MaxFramesInFlight = 2;
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
    std::optional<Textures> textures;
    std::optional<BindlessResources> bindlessResources;

    VkDescriptorSetLayout sceneDescriptorSetLayout;

    explicit VulkanBackend() {}
    explicit VulkanBackend(GLFWwindow* window);
    // TODO: init?
    auto deinit() -> void;

    auto currentFrame() -> FrameCtx&;
    auto newFrame() -> Frame;
    auto endFrame(Frame&& frame) -> FrameStats;

    auto render(const Frame& frame, CompiledRenderGraph& compiledRenderGraph, Scene& scene) -> void;

    auto immediateSubmit(std::function<void(VkCommandBuffer)>&& f) -> void;
    auto copyBuffer(VkBuffer src, VkBuffer dst, VkBufferCopy copyRegion) -> void;
    auto copyBufferWithStaging(void* data, size_t size, VkBuffer dst, VkBufferCopy copyRegion = VkBufferCopy()) -> void;

    auto allocateBuffer(VkBufferCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags,
        VkMemoryPropertyFlags requiredFlags) -> AllocatedBuffer;
    auto allocateImage(VkImageCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags,
        VkMemoryPropertyFlags requiredFlags, VkImageAspectFlags aspectFlags) -> AllocatedImage;

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