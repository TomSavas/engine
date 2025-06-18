#pragma once 

#include "engine.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/image.h"
#include "rhi/vulkan/utils/texture.h"
#include "rhi/vulkan/vulkan.h"
#include "rhi/vulkan/descriptors.h"
#include "rhi/vulkan/shader.h"
#include "rhi/vulkan/renderpass.h"

#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"

#include <chrono>
#include <glm/glm.hpp>
#include <functional>

struct Stats
{
    uint64_t finishedFrameCount = 0;
};

struct FrameData
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
};

// struct PushConstants
// {
//     glm::mat4 model;
//     glm::vec4 color;
//     VkDeviceAddress vertexBufferAddr;
//     VkDeviceAddress perModelDataBufferAddr;
//     VkDeviceAddress shadowData;
//     int shadowMapIndex;
// };

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
class CompiledRenderGraph;

class VulkanBackend;
std::optional<VulkanBackend*> initVulkanBackend();

struct Frame 
{
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    uint64_t frameIndex;
};

struct VulkanBackend
{
    VulkanBackend() {}
    
    VulkanBackend(GLFWwindow* window);
    // TODO: init?
    void deinit();

    void registerCallbacks();
    void draw(Scene& scene);

    FrameData& currentFrame();

    Frame newFrame();
    void endFrame(Frame);

    bool shutdownRequested = false;

    std::optional<Textures> textures;
    std::optional<BindlessResources> bindlessResources;

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

    // Frames
    static constexpr int MaxFramesInFlight = 2;
    FrameData frames[MaxFramesInFlight];
    uint64_t currentFrameNumber = 0;

    // Allocators
    VmaAllocator allocator;

    DescriptorAllocator descriptorAllocator;
    DescriptorAllocator bindlessDescPoolAllocator;
    // TEMP: probably a better place for this?
    VkDescriptorSet drawDescriptorSet;
    VkDescriptorSetLayout drawDescriptorSetLayout;

    VkDescriptorSet bindlessTexDesc;
    VkDescriptorSetLayout bindlessTexDescLayout;

    // Caches
    ShaderModuleCache shaderModuleCache;

    // Profiler
    TracyVkCtx tracyCtx;

    // Debug
    Stats       stats;

    std::unordered_map<Mesh*, GpuMeshBuffers> gpuMeshBuffers;

    //
    // RenderGraph graph;

    VkDescriptorSetLayout sceneDescriptorSetLayout;

    void render(CompiledRenderGraph& compiledRenderGraph, Scene& scene);

    void immediateSubmit(std::function<void (VkCommandBuffer)>&& f);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkBufferCopy copyRegion);
    void copyBufferWithStaging(void* data, size_t size, VkBuffer dst, VkBufferCopy copyRegion = VkBufferCopy());

    AllocatedBuffer allocateBuffer(VkBufferCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags, VkMemoryPropertyFlags requiredFlags);
    AllocatedImage allocateImage(VkImageCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags, VkMemoryPropertyFlags requiredFlags, VkImageAspectFlags aspectFlags);

    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);

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