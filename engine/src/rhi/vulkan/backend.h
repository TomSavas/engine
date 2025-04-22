#pragma once 

#include "engine.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/images.h"
#include "rhi/vulkan/descriptors.h"
#include "rhi/vulkan/shader.h"
#include "rhi/vulkan/renderpass.h"

#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"

#include <glm/glm.hpp>
#include <functional>

#define ZoneScopedCpuGpuAuto(name) ZoneScopedCpuGpu(tracyCtx, tracyCmdBuffer, name)
#define ZoneScopedCpuGpu(ctx, cmd, name) \
do                                       \
{                                        \
    ZoneScopedN(name " CPU");            \
    TracyVkZone(ctx, cmd, name " GPU")   \
} while (0)

#define ZoneScopedCpuGpuAutoStr(name) ZoneScopedCpuGpuStr(tracyCtx, tracyCmdBuffer, name)
#define ZoneScopedCpuGpuStr(ctx, cmd, name) \
do                                         \
{                                          \
    ZoneScopedN((name + " CPU").c_str());            \
    TracyVkZone(ctx, cmd, (name + " GPU").c_str())   \
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

struct PushConstants
{
    glm::mat4 model;
    glm::vec4 color;
    VkDeviceAddress vertexBufferAddr;  
};

// TEMP(savas): temporary solution for quick drawing of meshes
struct GpuMeshBuffers 
{
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

class GLFWwindow;
class Scene;
class Mesh;

struct VulkanBackend 
{
    VulkanBackend(GLFWwindow* window);
    // TODO: init?
    void deinit();

    void registerCallbacks();
    void draw(Scene& scene);

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
    AllocatedImage depthImage;

    // TEMP: concrete compute pipeline we for test render
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VkPipelineLayout trianglePipelineLayout;
    VkPipeline trianglePipeline;

    VkPipelineLayout infGridPipelineLayout;
    VkPipeline infGridPipeline;

    VkPipelineLayout meshPipelineLayout;
    VkPipeline meshPipeline;
    VkPipeline noDepthMeshPipeline;

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

    std::unordered_map<Mesh*, GpuMeshBuffers> gpuMeshBuffers;

    //
    RenderGraph graph;

    VkDescriptorSetLayout sceneDescriptorSetLayout;

    void immediateSubmit(std::function<void (VkCommandBuffer)>&& f);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkBufferCopy copyRegion);
    void copyBufferWithStaging(void* data, size_t size, VkBuffer dst, VkBufferCopy copyRegion = VkBufferCopy());

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
};
