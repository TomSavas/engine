#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "rhi/vulkan/backend.h"

#include "backend.h"
#include "scene.h"

#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/utils/image.h"
#include "rhi/vulkan/utils/buffer.h"

#include "rhi/vulkan/renderpass.h"

#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "GLFW/glfw3.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include <chrono>
#include <cmath>

std::optional<VulkanBackend> initVulkanBackend()
{
    if (!glfwInit()) {
        std::println("Failed initing GLFW");
        return {};
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Engine", NULL, NULL);

    VulkanBackend backend(window);
    backend.registerCallbacks();

    return backend;
}

Frame VulkanBackend::newFrame() 
{
    return Frame
    {
        .startTime = std::chrono::high_resolution_clock::now(),
        .frameIndex = currentFrameNumber
    };
}

void VulkanBackend::endFrame(Frame) 
{
    
}

VulkanBackend::VulkanBackend(GLFWwindow* window) : 
    window(window)
{
    int32_t width;
    int32_t height;
    glfwGetFramebufferSize(window, &width, &height);

    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    scissor.offset = { 0, 0 };
    scissor.extent =
    { 
        static_cast<uint32_t>(viewport.width),
        static_cast<uint32_t>(viewport.height)
    };

    initVulkan();
    initSwapchain();
    initCommandBuffers();
    initSyncStructs();
    initDescriptors();
    initPipelines();
    initImgui();
    
    initProfiler();
}

void VulkanBackend::deinit()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

void VulkanBackend::registerCallbacks()
{
}

void VulkanBackend::initVulkan()
{
    vkb::InstanceBuilder builder;
    vkbInstance = builder.set_app_name("engine")
#ifdef DEBUG
        .request_validation_layers(true)
#else //DEBUG
        .request_validation_layers(false)
#endif //DEBUG
        .require_api_version(1, 3, 0)
        .use_default_debug_messenger()
        .build()
        .value();
    instance = vkbInstance.instance;
    debugMessenger = vkbInstance.debug_messenger;

    VkResult err = glfwCreateWindowSurface(instance, window, NULL, &surface);
    if (err != VK_SUCCESS) 
    {
        std::println("Failed creating surface");
    }

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = nullptr;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.runtimeDescriptorArray = true;
    features12.descriptorBindingStorageBufferUpdateAfterBind = true;
    features12.descriptorBindingSampledImageUpdateAfterBind = true;
    features12.descriptorBindingVariableDescriptorCount = true;

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.pNext = nullptr;
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceFeatures features = {};
    features.geometryShader = true;
    features.multiDrawIndirect = true;
    features.drawIndirectFirstInstance = true;
    features.depthClamp = true;

    vkb::PhysicalDeviceSelector selector { vkbInstance };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features(features)
        .set_required_features_12(features12)
        .set_required_features_13(features13)
        .set_surface(surface)
        .select()
        .value();
    vkb::DeviceBuilder deviceBuilder { physicalDevice };
    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures = {};
    shaderDrawParametersFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shaderDrawParametersFeatures.pNext = nullptr;
    shaderDrawParametersFeatures.shaderDrawParameters = VK_TRUE;
    deviceBuilder.add_pNext(&shaderDrawParametersFeatures);
    vkb::Device vkbDevice = deviceBuilder.build().value();
    gpu = physicalDevice.physical_device;
    device = vkbDevice.device;

    gpuProperties = vkbDevice.physical_device.properties;

    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = gpu;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}

void VulkanBackend::initSwapchain()
{
    vkDeviceWaitIdle(device);

    vkb::SwapchainBuilder builder { gpu, device, surface };
    vkb::Swapchain vkbSwapchain = builder
        .use_default_format_selection()
        //.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
        //.set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        //.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
        .set_desired_extent(viewport.width, viewport.height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
    swapchainImageFormat = vkbSwapchain.image_format;

    //swapchainDeinitQueue.enqueue([=]() {
    //    LOG_CALL(vkDestroySwapchainKHR(device, swapchain, nullptr));
    //});

    //VkExtent3D depthImageExtent(viewport.width, viewport.height, 1.f);
    //depthFormat = VK_FORMAT_D32_SFLOAT;

    //VkImageCreateInfo depthImageInfo = imageCreateInfo(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

    //VmaAllocationCreateInfo depthImageAlloc = {};
    //depthImageAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    //depthImageAlloc.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    //vmaCreateImage(allocator, &depthImageInfo, &depthImageAlloc, &depthImage.image, &depthImage.allocation, nullptr);

    //swapchainDeinitQueue.enqueue([=]() {
    //    LOG_CALL(vmaDestroyImage(allocator, depthImage.image, depthImage.allocation));
    //});

    //VkImageViewCreateInfo depthViewInfo = imageViewCreateInfo(depthFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    //VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView));

    //swapchainDeinitQueue.enqueue([=]() {
    //    LOG_CALL(vkDestroyImageView(device, depthImageView, nullptr));
    //});
    
    // Backbuffer
    backbufferImage.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    backbufferImage.extent = { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height), 1 };
    VkImageUsageFlags backbufferUsageFlags = {};
    backbufferUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    backbufferUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    backbufferUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    backbufferUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto imgInfo = vkutil::init::imageCreateInfo(backbufferImage.format, backbufferUsageFlags, backbufferImage.extent);

    VmaAllocationCreateInfo allocInfo = {};
    // TODO: probably want this in tracy and on various debug tools in imgui
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vmaCreateImage(allocator, &imgInfo, &allocInfo, &backbufferImage.image, &backbufferImage.allocation, nullptr);

    auto imgViewInfo = vkutil::init::imageViewCreateInfo(backbufferImage.format, backbufferImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device, &imgViewInfo, nullptr, &backbufferImage.view));

    // Depth
    depthImage.format = VK_FORMAT_D32_SFLOAT;
    depthImage.extent = backbufferImage.extent;
    VkImageUsageFlags depthUsageFlags = {};
    depthUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    imgInfo = vkutil::init::imageCreateInfo(depthImage.format, depthUsageFlags, depthImage.extent);

    allocInfo = {};
    // TODO: probably want this in tracy and on various debug tools in imgui
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vmaCreateImage(allocator, &imgInfo, &allocInfo, &depthImage.image, &depthImage.allocation, nullptr);

    imgViewInfo = vkutil::init::imageViewCreateInfo(depthImage.format, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(device, &imgViewInfo, nullptr, &depthImage.view));

    // TODO: delete later
}

void VulkanBackend::initCommandBuffers()
{
    auto commandPoolInfo = vkutil::init::commandPoolCreateInfo(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    auto cmdAllocInfo = vkutil::init::commandBufferAllocateInfo(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_NULL_HANDLE);
    for (int i = 0; i < MaxFramesInFlight; i++) {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].cmdPool));
        cmdAllocInfo.commandPool = frames[i].cmdPool;
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].cmdBuffer));
    }

    // TEMP: move somewhere else. Immediate context
    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &immediateCmdPool));
    cmdAllocInfo = vkutil::init::commandBufferAllocateInfo(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, immediateCmdPool);
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &immediateCmdBuffer));
    
    //deinitQueue.enqueue([=]() {
    //        LOG_CALL(
    //                for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    //                vkDestroyCommandPool(device, inFlightFrames[i].cmdPool, nullptr);
    //                }
    //                );
    //        });

    //VkCommandPoolCreateInfo uploadCmdPoolInfo = commandPoolCreateInfo(graphicsQueueFamily, 0);
    //VK_CHECK(vkCreateCommandPool(device, &uploadCmdPoolInfo, nullptr, &uploadCtx.cmdPool));

    //VkCommandBufferAllocateInfo uploadCmdAllocInfo = commandBufferAllocateInfo(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, uploadCtx.cmdPool);
    //VK_CHECK(vkAllocateCommandBuffers(device, &uploadCmdAllocInfo, &uploadCtx.cmdBuffer));

    //deinitQueue.enqueue([=]() {
    //        LOG_CALL(vkDestroyCommandPool(device, uploadCtx.cmdPool, nullptr));
    //});
}

void VulkanBackend::initSyncStructs()
{
    // We want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
    auto fenceCreateInfo = vkutil::init::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    // For the semaphores we don't need any flags
    auto semCreateInfo = vkutil::init::semaphoreCreateInfo(0);

    for (int i = 0; i < MaxFramesInFlight; i++) {
        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(device, &semCreateInfo, nullptr, &frames[i].presentSem));
        VK_CHECK(vkCreateSemaphore(device, &semCreateInfo, nullptr, &frames[i].renderSem));
    }

    // TEMP: move somewhere else. Immediate context
    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &immediateFence));

    //deinitQueue.enqueue([=](){
    //    LOG_CALL(
    //        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    //            vkDestroyFence(device, inFlightFrames[i].renderFence, nullptr);
    //            vkDestroySemaphore(device, inFlightFrames[i].presentSem, nullptr);
    //            vkDestroySemaphore(device, inFlightFrames[i].renderSem, nullptr);
    //        }
    //    );
    //});

    //VkFenceCreateInfo uploadFenceCreateInfo = vkutil::init::fenceCreateInfo(0);
    //VK_CHECK(vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &uploadCtx.uploadFence));
    //deinitQueue.enqueue([=](){
    //    LOG_CALL(vkDestroyFence(device, uploadCtx.uploadFence, nullptr));
    //});
}

// TODO(savas): REMOVE ME! Testing purposes only
struct SceneUniforms 
{
    glm::mat4 view;
    glm::mat4 projection;
};
static SceneUniforms sceneUniforms;
static AllocatedBuffer sceneUniformBuffer;
static VkDescriptorSet sceneDescriptorSet;

void VulkanBackend::initDescriptors()
{
    VkDescriptorPoolSize poolSizes[] =
    {
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
    };
    descriptorAllocator.init(device, 20, poolSizes);

    // Allocating for the "draw" descriptor set
    {
        DescriptorSetLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        drawDescriptorSetLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
        drawDescriptorSet = descriptorAllocator.allocate(device, drawDescriptorSetLayout);

        VkDescriptorImageInfo descriptorImage = vkutil::init::descriptorImageInfo(VK_NULL_HANDLE, backbufferImage.view, VK_IMAGE_LAYOUT_GENERAL);
        VkWriteDescriptorSet descriptorImageWrite = vkutil::init::writeDescriptorImage(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, drawDescriptorSet, &descriptorImage, 0);
        vkUpdateDescriptorSets(device, 1, &descriptorImageWrite, 0, nullptr);
    }

    {
        auto bufInfo = vkutil::init::bufferCreateInfo(sizeof(SceneUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        sceneUniformBuffer = allocateBuffer(bufInfo, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    {
        DescriptorSetLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        // sceneDescriptorSetLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        sceneDescriptorSetLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
        sceneDescriptorSet = descriptorAllocator.allocate(device, sceneDescriptorSetLayout);

        VkDescriptorBufferInfo descriptorBufferInfo = vkutil::init::descriptorBufferInfo(sceneUniformBuffer.buffer, 0, sizeof(SceneUniforms));
        VkWriteDescriptorSet descriptorImageWrite = vkutil::init::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sceneDescriptorSet, &descriptorBufferInfo, 0);

        vkUpdateDescriptorSets(device, 1, &descriptorImageWrite, 0, nullptr);
    }

    // Bindless descriptor set pool
    const uint32_t maxBindlessResourceCount = 10000;
    {
        VkDescriptorPoolSize poolSizes[] = 
        {
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = maxBindlessResourceCount },
        };
        bindlessDescPoolAllocator.init(device, maxBindlessResourceCount, poolSizes,
                                        VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
    }

    // Bindless descriptor set
    {
        const VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | 
                                                 VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | 
                                                 VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = 
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorSetCount = 1,
            // TODO: This should be an actual array that actually depends on how many descriptors we have
            // For now it's only textures tho.
            .pDescriptorCounts = &maxBindlessResourceCount
        };
        
        DescriptorSetLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindlessFlags, maxBindlessResourceCount);
        // Add more bindings for buffers, etc.
        bindlessTexDescLayout = builder.build(device, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
        bindlessTexDesc = bindlessDescPoolAllocator.allocate(device, bindlessTexDescLayout, &variableDescriptorCountAllocInfo);

        // VkDescriptorImageInfo descriptorImage = vkutil::init::descriptorImageInfo(VK_NULL_HANDLE, backbufferImage.view, VK_IMAGE_LAYOUT_GENERAL);
        // VkWriteDescriptorSet descriptorImageWrite = vkutil::init::writeDescriptorImage(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, drawDescriptorSet, &descriptorImage, 0);
        // vkUpdateDescriptorSets(device, 1, &descriptorImageWrite, 0, nullptr);
    }

}

void VulkanBackend::initPipelines()
{
    ZoneScopedN("Build pipelines");
}

void VulkanBackend::initImgui()
{
    VkDescriptorPoolSize pool_sizes[] = 
        { 
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } 
        };

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    poolCreateInfo.maxSets = 1000;
    poolCreateInfo.poolSizeCount = (uint32_t)std::size(pool_sizes);
    poolCreateInfo.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &imguiPool));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
    imguiInitInfo.Instance = instance;
    imguiInitInfo.PhysicalDevice = gpu;
    imguiInitInfo.Device = device;
    imguiInitInfo.Queue = graphicsQueue;
    imguiInitInfo.DescriptorPool = imguiPool;
    imguiInitInfo.MinImageCount = 3;
    imguiInitInfo.ImageCount = 3;
    imguiInitInfo.UseDynamicRendering = true;

    //dynamic rendering parameters for imgui to use
    imguiInitInfo.PipelineRenderingCreateInfo = {};
    imguiInitInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    imguiInitInfo.PipelineRenderingCreateInfo.pNext = nullptr;

    imguiInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    imguiInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat;
	
    imguiInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&imguiInitInfo);

    ImGui_ImplVulkan_CreateFontsTexture();
}

void VulkanBackend::initProfiler()
{
    auto commandPoolInfo = vkutil::init::commandPoolCreateInfo(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    auto fenceCreateInfo = vkutil::init::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    for (int i = 0; i < MaxFramesInFlight; i++) {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].tracyCmdPool));

        auto cmdAllocInfo = vkutil::init::commandBufferAllocateInfo(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, frames[i].tracyCmdPool);
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].tracyCmdBuffer));

        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].tracyRenderFence));

        frames[i].tracyCtx = TracyVkContextCalibrated(instance, gpu, device, graphicsQueue, frames[i].tracyCmdBuffer, vkbInstance.fp_vkGetInstanceProcAddr, vkbInstance.fp_vkGetDeviceProcAddr);
    }
}

FrameData& VulkanBackend::currentFrame()
{
    return frames[currentFrameNumber % MaxFramesInFlight];
}

void VulkanBackend::draw(Scene& scene)
{
    ZoneScoped;
    constexpr uint64_t timeoutNs = 100'000'000'000'000;

    auto frame = currentFrame();
    auto cmd = frame.cmdBuffer;

    uint32_t swapchainImageIndex;
    {
        ZoneScopedN("Sync CPU");

        VK_CHECK(vkWaitForFences(device, 1, &frame.renderFence, true, timeoutNs));
        // TODO: move after swapchain regen... maybe?

        VK_CHECK(vkAcquireNextImageKHR(device, swapchain, timeoutNs,
            frame.presentSem, nullptr, &swapchainImageIndex));
        // TODO: if swapchain regen requested process, reacquire index and continue

        VK_CHECK(vkResetFences(device, 1, &frame.renderFence));
    }

    {
        ZoneScopedN("Sync Tracy");

        VK_CHECK(vkWaitForFences(device, 1, &frame.tracyRenderFence, true, timeoutNs));
        VK_CHECK(vkResetFences(device, 1, &frame.tracyRenderFence));
        {
            VK_CHECK(vkResetCommandBuffer(frame.tracyCmdBuffer, 0));
            auto cmdBeginInfo = vkutil::init::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            VK_CHECK(vkBeginCommandBuffer(frame.tracyCmdBuffer, &cmdBeginInfo));
        }
    }

    {
        ZoneScopedCpuGpuAuto("Record");

        VK_CHECK(vkResetCommandBuffer(cmd, 0));
        auto cmdBeginInfo = vkutil::init::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

        {
            ZoneScopedCpuGpuAuto("Transition resources");
            
            vkutil::image::transitionImage(cmd, backbufferImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            vkutil::image::transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
            vkutil::image::transitionImage(cmd, backbufferImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        VkExtent2D swapchainSize { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };

        // Update scene descriptor set
        {
            ZoneScopedCpuGpuAuto("Memcpy SceneUniforms to GPU");

            sceneUniforms.view = scene.activeCamera->view();
            sceneUniforms.projection = scene.activeCamera->proj();

            uint8_t* dataOnGpu;
            vmaMapMemory(allocator, sceneUniformBuffer.allocation, (void**)&dataOnGpu);
            memcpy(dataOnGpu, &sceneUniforms, sizeof(sceneUniforms));
            vmaUnmapMemory(allocator, sceneUniformBuffer.allocation);
        }

        // TODO: this should be multithreaded
        for (RenderPass& pass : graph.renderpasses)
        {
            ZoneScoped;
            ZoneName(pass.debugName.c_str(), pass.debugName.size());

            // TODO: transition resources here

            // Get the attachments from rendergraph
            if (pass.pipeline) 
            {
                vkCmdBeginRendering(cmd, &pass.renderingInfo);

                vkCmdBindPipeline(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipeline);

                if (pass.debugName == "base pass")
                {
                    vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 0, 1, &sceneDescriptorSet, 0, nullptr);
                    vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1, &bindlessTexDesc, 0, nullptr);
                }

                vkCmdSetViewport(cmd, 0, 1, &viewport);
                vkCmdSetScissor(cmd, 0, 1, &scissor);

                pass.draw(cmd, pass);

                vkCmdEndRendering(cmd);
            }
            else 
            {
                pass.draw(cmd, pass);
            }
        }

        VkExtent2D backbufferSize { backbufferImage.extent.width, backbufferImage.extent.height };
        {
            ZoneScopedCpuGpuAuto("Blit to swapchain");
            
            vkutil::image::transitionImage(cmd, backbufferImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            vkutil::image::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkutil::image::blitImageToImage(cmd, backbufferImage.image, backbufferSize, swapchainImages[swapchainImageIndex], swapchainSize);
        }

        {
            ZoneScopedCpuGpuAuto("Render Imgui");

            ImGui::Render();

            vkutil::image::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(swapchainImageViews[swapchainImageIndex], nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(swapchainSize, &colorAttachmentInfo, 1, nullptr);

            vkCmdBeginRendering(cmd, &renderingInfo);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
            vkCmdEndRendering(cmd);
        }
        
        vkutil::image::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VK_CHECK(vkEndCommandBuffer(cmd));
    }

    {
        ZoneScopedCpuGpuAuto("Submit");

        auto cmdInfo = vkutil::init::commandBufferSubmitInfo(cmd);
        auto waitInfo = vkutil::init::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame.presentSem);
        auto signalInfo = vkutil::init::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.renderSem);
        auto submit = vkutil::init::submitInfo2(&cmdInfo, &waitInfo, &signalInfo);

        VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, frame.renderFence));
    }

    {
        ZoneScopedCpuGpuAuto("Present");

        auto presentInfo = vkutil::init::presentInfo(&swapchain, &frame.renderSem, &swapchainImageIndex);
        VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));
    }

    TracyVkCollect(frame.tracyCtx, frame.tracyCmdBuffer);

    VK_CHECK(vkEndCommandBuffer(frame.tracyCmdBuffer));
    auto cmdInfo = vkutil::init::commandBufferSubmitInfo(frame.tracyCmdBuffer);
    auto submit = vkutil::init::submitInfo2(&cmdInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, frame.tracyRenderFence));

    FrameMark;

    currentFrameNumber++;
    stats.finishedFrameCount++;
}

void VulkanBackend::immediateSubmit(std::function<void (VkCommandBuffer)>&& f)
{
    // TODO: instrument this
    VK_CHECK(vkResetFences(device, 1, &immediateFence));   
    VK_CHECK(vkResetCommandBuffer(immediateCmdBuffer, 0));

    {
        VkCommandBuffer cmd = immediateCmdBuffer;

        auto cmdBeginInfo = vkutil::init::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
        
        f(cmd);

        VK_CHECK(vkEndCommandBuffer(cmd));

        auto cmdInfo = vkutil::init::commandBufferSubmitInfo(cmd);
        auto submit = vkutil::init::submitInfo2(&cmdInfo, nullptr, nullptr);
        VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, immediateFence));

        // NOTE: I think this is better moved to the start, so that we don't immedeately block
        VK_CHECK(vkWaitForFences(device, 1, &immediateFence, true, 9999999999));
    }
}

void VulkanBackend::copyBuffer(VkBuffer src, VkBuffer dst, VkBufferCopy copyRegion) 
{
    immediateSubmit([&](VkCommandBuffer cmd) 
        {
            vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);
        }
    );
}

void VulkanBackend::copyBufferWithStaging(void* data, size_t size, VkBuffer dst, VkBufferCopy copyRegion) 
{
    auto bufInfo = vkutil::init::bufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    AllocatedBuffer staging = allocateBuffer(bufInfo, VMA_MEMORY_USAGE_CPU_ONLY,
        VMA_ALLOCATION_CREATE_MAPPED_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VmaAllocationInfo stagingInfo;
    vmaGetAllocationInfo(allocator, staging.allocation, &stagingInfo);

    void* stagingData = stagingInfo.pMappedData;
    memcpy(stagingData, data, size);

    copyRegion.size = size;
    copyBuffer(staging.buffer, dst, copyRegion);

    // TODO: release staging data
}

AllocatedBuffer VulkanBackend::allocateBuffer(VkBufferCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags, VkMemoryPropertyFlags requiredFlags)
{
    AllocatedBuffer buffer;
    
    VmaAllocationCreateInfo allocInfo
    {
        .flags = flags,
        .usage = usage,
        .requiredFlags = requiredFlags,    
    };
    VK_CHECK(vmaCreateBuffer(allocator, &info, &allocInfo, &buffer.buffer, &buffer.allocation, nullptr));

    return buffer;
}

AllocatedImage VulkanBackend::allocateImage(VkImageCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags, VkMemoryPropertyFlags requiredFlags, VkImageAspectFlags aspectFlags)
{
    AllocatedImage image;
    image.format = info.format;
    image.extent = info.extent;
    
    VmaAllocationCreateInfo allocInfo
    {
        .flags = flags,
        .usage = usage,
        .requiredFlags = requiredFlags,    
    };
    VK_CHECK(vmaCreateImage(allocator, &info, &allocInfo, &image.image, &image.allocation, nullptr));

    auto imgViewInfo = vkutil::init::imageViewCreateInfo(image.format, image.image, aspectFlags);
    VK_CHECK(vkCreateImageView(device, &imgViewInfo, nullptr, &image.view));

    return image;
}
