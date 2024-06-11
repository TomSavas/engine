#include "imgui.h"
#include "utils/images.h"
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include "rhi/vulkan/backend.h"

#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/utils/images.h"

#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <cmath>

// TEMP: test for tracy allocations
void* operator new(std::size_t size) noexcept(false)
{
    void* ptr = std::malloc(size);
    TracyAlloc(ptr, size);

    return ptr;
}
void operator delete(void* ptr)
{
    TracyFree(ptr);
    std::free(ptr);
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
    viewport.maxDepth = 0.f;

    scissor.offset = { 0, 0 };
    scissor.extent =
    { 
        static_cast<uint32_t>(viewport.width),
        static_cast<uint32_t>(viewport.height)
    };

    // NOTE: commented out because not using renderpasses
    initVulkan();
    initSwapchain();
    initCommandBuffers();
    //initDefaultRenderpass();
    //initFramebuffers();
    initSyncStructs();
    initDescriptors();
    initPipelines();
    initImgui();
    
    initProfiler();
}

void VulkanBackend::deinit()
{
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

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.pNext = nullptr;
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    vkb::PhysicalDeviceSelector selector { vkbInstance };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
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

//void VulkanBackend::initFramebuffers()
//{
//    VkFramebufferCreateInfo fbInfo = {};
//    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
//    fbInfo.pNext = nullptr;
//    fbInfo.renderPass = defaultRenderpass;
//    fbInfo.attachmentCount = 1;
//    fbInfo.width = viewport.width;
//    fbInfo.height = viewport.height;
//    fbInfo.layers = 1;
//
//    const uint32_t swapchainImageCount = swapchainImages.size();
//    framebuffers = std::vector<VkFramebuffer(swapchainImageCount);
//
//    for (uint32_t i = 0; i < swapchainImageCount; ++i) 
//    {
//        VkImageView attachments[] = { swapchainImageViews[i], depthImageView };
//    }
//}

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

void VulkanBackend::initDescriptors()
{
    VkDescriptorPoolSize poolSizes[] =
    {
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
    };
    descriptorAllocator.init(device, 10, poolSizes);

    // Allocating for the "draw" descriptor set
    DescriptorSetLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    drawDescriptorSetLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
    drawDescriptorSet = descriptorAllocator.allocate(device, drawDescriptorSetLayout);

    VkDescriptorImageInfo descriptorImage = vkutil::init::descriptorImageInfo(VK_NULL_HANDLE, backbufferImage.view, VK_IMAGE_LAYOUT_GENERAL);
    VkWriteDescriptorSet descriptorImageWrite = vkutil::init::writeDescriptorImage(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, drawDescriptorSet, &descriptorImage, 0);

    vkUpdateDescriptorSets(device, 1, &descriptorImageWrite, 0, nullptr);
}

void VulkanBackend::initPipelines()
{
    ZoneScopedN("Build pipelines");
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(&drawDescriptorSetLayout, 1);
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    std::optional<ShaderModule*> shaderModule = shaderModuleCache.loadModule(device, SHADER_PATH("gradient.comp.glsl"));
    if (!shaderModule)
    {
        std::println("Error initialising pipeline!");
        return;
    }

    VkPipelineShaderStageCreateInfo pipelineShaderStageInfo = vkutil::init::shaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, (*shaderModule)->module);
    VkComputePipelineCreateInfo pipelineCreateInfo = vkutil::init::computePipelineCreateInfo(pipelineLayout, pipelineShaderStageInfo);
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline));

    // TEMP(savas): triangle pipeline
    pipelineLayoutInfo = vkutil::init::layoutCreateInfo();
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &trianglePipelineLayout));

    std::optional<ShaderModule*> vertexShader = shaderModuleCache.loadModule(device, SHADER_PATH("colored_triangle.vert.glsl"));
    std::optional<ShaderModule*> fragmentShader = shaderModuleCache.loadModule(device, SHADER_PATH("colored_triangle.frag.glsl"));
    if (!vertexShader || !fragmentShader)
    {
        std::println("Error loading shaders");
        return;
    }

    trianglePipeline = PipelineBuilder()
        .shaders((*vertexShader)->module, (*fragmentShader)->module)
        .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .polyMode(VK_POLYGON_MODE_FILL)
        .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
        .disableMultisampling()
        .disableBlending()
        .disableDepthTest()
        .colorAttachmentFormat(backbufferImage.format)
        .depthFormat(VK_FORMAT_UNDEFINED)
        .build(device, trianglePipelineLayout);
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
    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &tracyCmdPool));
    auto cmdAllocInfo = vkutil::init::commandBufferAllocateInfo(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, tracyCmdPool);
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &tracyCmdBuffer));

    auto fenceInfo = vkutil::init::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &tracyRenderFence));

    // TODO(savas): change to calibrated context
    tracyCtx = TracyVkContext(gpu, device, graphicsQueue, tracyCmdBuffer);
}

FrameData& VulkanBackend::currentFrame()
{
    return frames[currentFrameNumber % MaxFramesInFlight];
}

void VulkanBackend::draw()
{
    ZoneScoped;
    constexpr int timeoutNs = 1'000'000'000;

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

        VK_CHECK(vkWaitForFences(device, 1, &tracyRenderFence, true, timeoutNs));
        VK_CHECK(vkResetFences(device, 1, &tracyRenderFence));
        {
            VK_CHECK(vkResetCommandBuffer(tracyCmdBuffer, 0));
            auto cmdBeginInfo = vkutil::init::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            VK_CHECK(vkBeginCommandBuffer(tracyCmdBuffer, &cmdBeginInfo));
        }
    }

    {
        ZoneScopedCpuGpuAuto("Record");

        VK_CHECK(vkResetCommandBuffer(cmd, 0));
        auto cmdBeginInfo = vkutil::init::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

        {
            ZoneScopedCpuGpuAuto("Compute gradient");

            vkutil::image::transitionImage(cmd, backbufferImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &drawDescriptorSet, 0, nullptr);
            vkCmdDispatch(cmd, std::ceil(viewport.width / 16.f), std::ceil(viewport.height / 16.f), 1);
        }        

        VkExtent2D swapchainSize { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };
        {
            ZoneScopedCpuGpuAuto("Render triangle");

            vkutil::image::transitionImage(cmd, backbufferImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(backbufferImage.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(swapchainSize, &colorAttachmentInfo, nullptr);

            VkViewport viewport = {};
            viewport.width = swapchainSize.width;
            viewport.height = swapchainSize.height;
            viewport.maxDepth = 1.f;

            VkRect2D scissor = {};
            scissor.extent = swapchainSize;

            vkCmdBeginRendering(cmd, &renderingInfo);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);
            vkCmdDraw(cmd, 3, 1, 0, 0);

            vkCmdEndRendering(cmd);
        }

        VkExtent2D backbufferSize { backbufferImage.extent.width, backbufferImage.extent.height };
        {
            ZoneScopedCpuGpuAuto("Blit to swapchain");
            
            //vkutil::image::transitionImage(cmd, backbufferImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            vkutil::image::transitionImage(cmd, backbufferImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            vkutil::image::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkutil::image::blitImageToImage(cmd, backbufferImage.image, backbufferSize, swapchainImages[swapchainImageIndex], swapchainSize);
        }

        {
            ZoneScopedCpuGpuAuto("Render Imgui");

            vkutil::image::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(swapchainImageViews[swapchainImageIndex], nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(swapchainSize, &colorAttachmentInfo, nullptr);

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

    TracyVkCollect(tracyCtx, tracyCmdBuffer);

    VK_CHECK(vkEndCommandBuffer(tracyCmdBuffer));
    auto cmdInfo = vkutil::init::commandBufferSubmitInfo(tracyCmdBuffer);
    auto submit = vkutil::init::submitInfo2(&cmdInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, tracyRenderFence));

    FrameMark;

    //TracyVkDestroy(tracyCtx);

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
