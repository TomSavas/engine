#pragma once 

#include "engine.h"
#include "rhi/vulkan/utils/images.h"
#include "rhi/vulkan/descriptors.h"
#include "rhi/vulkan/shader.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"

#include <functional>

#define ZoneScopedCpuGpuAuto(name) ZoneScopedCpuGpu(tracyCtx, tracyCmdBuffer, name)
#define ZoneScopedCpuGpu(ctx, cmd, name) \
do                                       \
{                                        \
    ZoneScopedN(name " CPU");            \
    TracyVkZone(ctx, cmd, name " GPU")   \
} while (0)

#include <iostream>
#define VK_CHECK(x)                            \
    do                                         \
{                                              \
    VkResult err = x;                          \
    if (err)                                   \
    {                                          \
        std::cout << "Vulkan error: " << err << std::endl; \
        BREAKPOINT;                            \
    }                                          \
} while (0)

struct Stats
{
    uint64_t finishedFrameCount = 0;
};

struct FrameData
{
    VkSemaphore presentSem;
    VkSemaphore renderSem;
    VkFence renderFence;

    VkCommandPool cmdPool;
    VkCommandBuffer cmdBuffer;
};

class GLFWwindow;
struct VulkanBackend 
{
    VulkanBackend(GLFWwindow* window);
    // TODO: init?
    void deinit();

    void registerCallbacks();
    void draw();

    FrameData& currentFrame();

    GLFWwindow* window;

    vkb::Instance vkbInstance;
    // Vk
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkSurfaceKHR surface;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkPhysicalDeviceProperties gpuProperties;

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;

    VkViewport  viewport;
    VkRect2D    scissor;

    // Swapchain
    VkSwapchainKHR           swapchain;
    VkFormat                 swapchainImageFormat;
    std::vector<VkImage>     swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    // Immediate ctx
    VkFence immediateFence;
    VkCommandPool immediateCmdPool;
    VkCommandBuffer immediateCmdBuffer;

    // Image we actually render to
    AllocatedImage backbufferImage;

    // TEMP: concrete compute pipeline we for test render
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VkPipelineLayout trianglePipelineLayout;
    VkPipeline trianglePipeline;

    // Frames
    static constexpr int MaxFramesInFlight = 2;
    FrameData frames[MaxFramesInFlight];
    uint64_t currentFrameNumber = 0;

    // Allocators
    VmaAllocator allocator;

    DescriptorAllocator descriptorAllocator;
    // TEMP: probably a better place for this?
    VkDescriptorSet drawDescriptorSet;
    VkDescriptorSetLayout drawDescriptorSetLayout;

    // Caches
    ShaderModuleCache shaderModuleCache;

    // Profiler
    TracyVkCtx tracyCtx;
    VkCommandPool tracyCmdPool;
    VkCommandBuffer tracyCmdBuffer;
    VkFence tracyRenderFence;

    // Debug
    Stats       stats;

private:
    void initVulkan();
    void initSwapchain();
    void initCommandBuffers();
    void initDefaultRenderpass();
    void initFramebuffers();
    void initSyncStructs();
    void initDescriptors();
    void initPipelines();
    void initImgui();
    void initProfiler();

    void immediateSubmit(std::function<void (VkCommandBuffer)>&& f);
};
