#pragma once

#include <chrono>
#include <functional>
#include <glm/glm.hpp>

#include "VkBootstrap.h"
#include "engine.h"
#include "result.hpp"
#include "rhi/renderpass.h"
#include "rhi/vulkan/descriptors.h"
#include "rhi/vulkan/shader.h"
#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/image.h"
#include "rhi/vulkan/utils/texture.h"
#include "rhi/vulkan/vulkan.h"
#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"
#include "vk_mem_alloc.h"

struct Stats
{
    uint64_t finishedFrameCount = 0;
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
class Scene;
struct Mesh;
struct CompiledRenderGraph;

class VulkanBackend;
enum class backendError {};
result::result<VulkanBackend*, backendError> initVulkanBackend();

struct FrameStats
{
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    uint64_t frameIndex;
    bool shutdownRequested;
};

struct Frame
{
    FrameStats stats;
    std::reference_wrapper<FrameCtx> ctx;
};

struct VulkanBackend
{
    VulkanBackend() {}

    VulkanBackend(GLFWwindow* window);
    // TODO: init?
    void deinit();

    FrameCtx& currentFrame();

    Frame newFrame();
    FrameStats endFrame(Frame&& frame);

    GLFWwindow* window;

    vkb::Instance vkbInstance;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkSurfaceKHR surface;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkPhysicalDeviceProperties gpuProperties;

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;

    VkQueue computeQueue;
    uint32_t computeQueueFamily;

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
    static constexpr int MaxFramesInFlight = 2;
    FrameCtx frames[MaxFramesInFlight];
    uint64_t currentFrameNumber = 0;

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

    void render(const Frame& frame, CompiledRenderGraph& compiledRenderGraph, Scene& scene);

    void immediateSubmit(std::function<void(VkCommandBuffer)>&& f);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkBufferCopy copyRegion);
    void copyBufferWithStaging(void* data, size_t size, VkBuffer dst, VkBufferCopy copyRegion = VkBufferCopy());

    AllocatedBuffer allocateBuffer(VkBufferCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags,
        VkMemoryPropertyFlags requiredFlags);
    AllocatedImage allocateImage(VkImageCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags,
        VkMemoryPropertyFlags requiredFlags, VkImageAspectFlags aspectFlags);

    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);

   private:
    void initVulkan();
    void initSwapchain();
    void initCommandBuffers();
    void initSyncStructs();
    void initDescriptors();
    void initImgui();
    void initProfiler();
};