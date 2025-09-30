#include "rhi/vulkan/backend.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "GLFW/glfw3.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "renderGraph.h"
#include "result.hpp"
#include "rhi/renderpass.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/image.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"
#include "scene.h"
#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

#include <chrono>
#include <cmath>

auto initVulkanBackend() -> result::result<VulkanBackend*, backendError>
{
    if (!glfwInit())
    {
        std::println("Failed initing GLFW");
        return result::fail(backendError{});
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Engine", NULL, NULL);

    VulkanBackend* backend = new VulkanBackend(window);

    return backend;
}

auto VulkanBackend::newFrame() -> Frame
{
    return Frame{
        .stats =
            {
                .startTime = std::chrono::high_resolution_clock::now(),
                .frameIndex = currentFrameNumber,
                .shutdownRequested = glfwWindowShouldClose(window),
                .pastFrameDt = 0.f,
            },
        .ctx = currentFrame(),
    };
}

auto VulkanBackend::endFrame(Frame&& frame) -> FrameStats
{
    FrameMark;
    currentFrameNumber++;
    stats.finishedFrameCount++;

    return frame.stats;
}

VulkanBackend::VulkanBackend(GLFWwindow* window) : window(window)
{
    i32 width;
    i32 height;
    glfwGetFramebufferSize(window, &width, &height);

    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<f32>(width);
    viewport.height = static_cast<f32>(height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    scissor.offset = {0, 0};
    scissor.extent = {static_cast<u32>(viewport.width), static_cast<u32>(viewport.height)};

    initVulkan();
    initSwapchain();
    initCommandBuffers();
    initSyncStructs();
    initDescriptors();
    initImgui();

    initProfiler();

    textures = Textures(*this);
    bindlessResources = BindlessResources(*this);
}

auto VulkanBackend::deinit() -> void
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

auto VulkanBackend::initVulkan() -> void
{
    vkb::InstanceBuilder builder;
    vkbInstance = builder
                      .set_app_name("engine")
#ifdef DEBUG
                      .request_validation_layers(true)
#else   // DEBUG
                      .request_validation_layers(false)
#endif  // DEBUG
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

    vkb::PhysicalDeviceSelector selector{vkbInstance};
    vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 3)
                                             .set_required_features(features)
                                             .set_required_features_12(features12)
                                             .set_required_features_13(features13)
                                             .set_surface(surface)
                                             .select()
                                             .value();
    vkb::DeviceBuilder deviceBuilder{physicalDevice};
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

    computeQueue = vkbDevice.get_queue(vkb::QueueType::compute).value();
    computeQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::compute).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = gpu;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}

auto VulkanBackend::initSwapchain() -> void
{
    vkDeviceWaitIdle(device);

    vkb::SwapchainBuilder builder{gpu, device, surface};
    vkb::Swapchain vkbSwapchain = builder
                                      .use_default_format_selection()
                                      //.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                      .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
                                      //.set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
                                      //.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                                      .set_desired_min_image_count(MaxFramesInFlight)
                                      .set_desired_extent(viewport.width, viewport.height)
                                      .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                      .build()
                                      .value();

    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
    swapchainImageFormat = vkbSwapchain.image_format;

    // Backbuffer
    backbufferImage.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    backbufferImage.extent = {static_cast<u32>(viewport.width), static_cast<u32>(viewport.height), 1};
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

    auto imgViewInfo = vkutil::init::imageViewCreateInfo(
        backbufferImage.format, backbufferImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device, &imgViewInfo, nullptr, &backbufferImage.view));
}

auto VulkanBackend::initCommandBuffers() -> void
{
    auto commandPoolInfo = vkutil::init::commandPoolCreateInfo(
        computeQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    auto cmdAllocInfo = vkutil::init::commandBufferAllocateInfo(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_NULL_HANDLE);
    for (i32 i = 0; i < MaxFramesInFlight; i++)
    {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].cmdComputePool));
        cmdAllocInfo.commandPool = frames[i].cmdComputePool;
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].cmdComputeBuffer));
    }

    commandPoolInfo = vkutil::init::commandPoolCreateInfo(
        graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    cmdAllocInfo = vkutil::init::commandBufferAllocateInfo(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_NULL_HANDLE);
    for (i32 i = 0; i < MaxFramesInFlight; i++)
    {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].cmdPool));
        cmdAllocInfo.commandPool = frames[i].cmdPool;
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].cmdBuffer));
    }

    // TEMP: move somewhere else. Immediate context
    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &immediateCmdPool));
    cmdAllocInfo = vkutil::init::commandBufferAllocateInfo(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, immediateCmdPool);
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &immediateCmdBuffer));
}

auto VulkanBackend::initSyncStructs() -> void
{
    // We want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command
    // (for the first frame)
    auto fenceCreateInfo = vkutil::init::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    // For the semaphores we don't need any flags
    auto semCreateInfo = vkutil::init::semaphoreCreateInfo(0);

    for (i32 i = 0; i < MaxFramesInFlight; i++)
    {
        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(device, &semCreateInfo, nullptr, &frames[i].presentSem));
        VK_CHECK(vkCreateSemaphore(device, &semCreateInfo, nullptr, &frames[i].renderSem));
    }

    // TEMP: move somewhere else. Immediate context
    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &immediateFence));
}

// TODO(savas): REMOVE ME! Testing purposes only
struct SceneUniforms
{
    glm::vec4 cameraPos;
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 lightDir;
    glm::vec4 time;
};
static SceneUniforms sceneUniforms;
static AllocatedBuffer sceneUniformBuffer;
static VkDescriptorSet sceneDescriptorSet;

auto VulkanBackend::initDescriptors() -> void
{
    VkDescriptorPoolSize poolSizes[] = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
    };
    descriptorAllocator.init(device, 20, poolSizes);

    {
        auto bufInfo = vkutil::init::bufferCreateInfo(sizeof(SceneUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        sceneUniformBuffer = allocateBuffer(bufInfo, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    {
        DescriptorSetLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        // sceneDescriptorSetLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        sceneDescriptorSetLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                                                             VK_SHADER_STAGE_GEOMETRY_BIT |
                                                             VK_SHADER_STAGE_COMPUTE_BIT);
        sceneDescriptorSet = descriptorAllocator.allocate(device, sceneDescriptorSetLayout);

        VkDescriptorBufferInfo descriptorBufferInfo = vkutil::init::descriptorBufferInfo(
            sceneUniformBuffer.buffer, 0, sizeof(SceneUniforms));
        VkWriteDescriptorSet descriptorImageWrite = vkutil::init::writeDescriptorBuffer(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sceneDescriptorSet, &descriptorBufferInfo, 0);

        vkUpdateDescriptorSets(device, 1, &descriptorImageWrite, 0, nullptr);
    }
}

auto VulkanBackend::initImgui() -> void
{
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000}, {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000}, {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    poolCreateInfo.maxSets = 1000;
    poolCreateInfo.poolSizeCount = (u32)std::size(pool_sizes);
    poolCreateInfo.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &imguiPool));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // ImGui::StyleColorsDark();
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.04f, 0.04f, 0.04f, 0.99f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.38f, 0.51f, 0.51f, 0.80f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.03f, 0.03f, 0.04f, 0.67f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.17f, 0.17f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.30f, 0.60f, 0.10f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.60f, 0.10f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.43f, 0.90f, 0.11f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.21f, 0.22f, 0.23f, 0.40f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.51f, 0.51f, 0.80f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.54f, 0.55f, 0.55f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.51f, 0.51f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.03f, 0.03f, 0.03f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.16f, 0.16f, 0.50f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.23f, 0.23f, 0.24f, 0.80f);
        colors[ImGuiCol_Tab] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
        colors[ImGuiCol_TabSelected] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.13f, 0.78f, 0.07f, 1.00f);
        colors[ImGuiCol_TabDimmed] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
        colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
        colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.10f, 0.60f, 0.12f, 1.00f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.14f, 0.87f, 0.05f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.30f, 0.60f, 0.10f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.23f, 0.78f, 0.02f, 1.00f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.46f, 0.47f, 0.46f, 0.06f);
        colors[ImGuiCol_TextLink] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavCursor] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.78f, 0.69f, 0.69f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        style.WindowRounding = 4.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 3.0f;
        style.PopupRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.WindowMenuButtonPosition = ImGuiDir_Right;
        style.ScrollbarSize = 10.0f;
        style.GrabMinSize = 10.0f;
        style.DockingSeparatorSize = 1.0f;
        style.SeparatorTextBorderSize = 2.0f;
    }
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
    imguiInitInfo.Instance = instance;
    imguiInitInfo.PhysicalDevice = gpu;
    imguiInitInfo.Device = device;
    imguiInitInfo.Queue = graphicsQueue;
    imguiInitInfo.DescriptorPool = imguiPool;
    imguiInitInfo.MinImageCount = 3;
    imguiInitInfo.ImageCount = 3;
    imguiInitInfo.UseDynamicRendering = true;

    // dynamic rendering parameters for imgui to use
    imguiInitInfo.PipelineRenderingCreateInfo = {};
    imguiInitInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    imguiInitInfo.PipelineRenderingCreateInfo.pNext = nullptr;

    imguiInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    imguiInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat;

    imguiInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&imguiInitInfo);

    // tex_data->DS = ImGui_ImplVulkan_AddTexture(tex_data->Sampler, tex_data->ImageView,
    // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // ImGui_ImplVulkan_CreateFontsTexture();
}

auto VulkanBackend::initProfiler() -> void
{
    auto commandPoolInfo = vkutil::init::commandPoolCreateInfo(
        graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    auto fenceCreateInfo = vkutil::init::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    for (i32 i = 0; i < MaxFramesInFlight; i++)
    {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].tracyCmdPool));

        auto cmdAllocInfo = vkutil::init::commandBufferAllocateInfo(
            1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, frames[i].tracyCmdPool);
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].tracyCmdBuffer));

        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].tracyRenderFence));

        frames[i].tracyCtx = TracyVkContextCalibrated(instance, gpu, device, graphicsQueue, frames[i].tracyCmdBuffer,
            vkbInstance.fp_vkGetInstanceProcAddr, vkbInstance.fp_vkGetDeviceProcAddr);
    }
}

auto VulkanBackend::currentFrame() -> FrameCtx& { return frames[currentFrameNumber % MaxFramesInFlight]; }

auto VulkanBackend::render(const Frame& frame, CompiledRenderGraph& graph, Scene& scene) -> void
{
    ZoneScoped;
    std::string profilerTag = std::format(
        "Rendering (frame={}, mod={})", currentFrameNumber, currentFrameNumber % MaxFramesInFlight);
    ZoneName(profilerTag.c_str(), profilerTag.size());

    constexpr u64 timeoutNs = 100'000'000'000'000;

    FrameCtx& frameCtx = frame.ctx;
    auto cmd = frameCtx.cmdBuffer;
    auto computeCmd = frameCtx.cmdComputeBuffer;

    u32 swapchainImageIndex;
    {
        ZoneScopedN("Sync CPU");

        VK_CHECK(vkWaitForFences(device, 1, &frameCtx.renderFence, true, timeoutNs));
        // TODO: move after swapchain regen... maybe?

        VK_CHECK(
            vkAcquireNextImageKHR(device, swapchain, timeoutNs, frameCtx.presentSem, nullptr, &swapchainImageIndex));
        // TODO: if swapchain regen requested process, reacquire index and continue

        VK_CHECK(vkResetFences(device, 1, &frameCtx.renderFence));
    }

    {
        ZoneScopedN("Sync Tracy");

        VK_CHECK(vkWaitForFences(device, 1, &frameCtx.tracyRenderFence, true, timeoutNs));
        VK_CHECK(vkResetFences(device, 1, &frameCtx.tracyRenderFence));
        {
            VK_CHECK(vkResetCommandBuffer(frameCtx.tracyCmdBuffer, 0));
            auto cmdBeginInfo = vkutil::init::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            VK_CHECK(vkBeginCommandBuffer(frameCtx.tracyCmdBuffer, &cmdBeginInfo));
        }
    }

    {
        ZoneScopedCpuGpuAuto("Record", frameCtx);

        VK_CHECK(vkResetCommandBuffer(cmd, 0));
        auto cmdBeginInfo = vkutil::init::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

        // TODO: this should go away once fully migrated over to render graph
        {
            ZoneScopedCpuGpuAuto("Transition resources", frameCtx);

            vkutil::image::transitionImage(
                cmd, backbufferImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            vkutil::image::transitionImage(
                cmd, backbufferImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        VkExtent2D swapchainSize{static_cast<u32>(viewport.width), static_cast<u32>(viewport.height)};

        // TODO: this should live as a separate pass in the render graph
        // Update scene descriptor set
        {
            ZoneScopedCpuGpuAuto("Memcpy SceneUniforms to GPU", frameCtx);

            sceneUniforms.cameraPos = glm::vec4(scene.activeCamera->position, 1.f);
            sceneUniforms.view = scene.activeCamera->view();
            sceneUniforms.projection = scene.activeCamera->proj();
            sceneUniforms.lightDir = glm::vec4(scene.lightDir, 5.f);
            static f64 time = 0.f;
            time += frame.stats.pastFrameDt;
            sceneUniforms.time = glm::vec4(time);

            u8* dataOnGpu;
            vmaMapMemory(allocator, sceneUniformBuffer.allocation, (void**)&dataOnGpu);
            memcpy(dataOnGpu, &sceneUniforms, sizeof(sceneUniforms));
            vmaUnmapMemory(allocator, sceneUniformBuffer.allocation);
        }

        {
            ZoneScopedCpuGpuAuto("Render graph", frameCtx);

            for (CompiledRenderGraph::Node& node : graph.nodes)
            {
                RenderPass& pass = node.pass;

                ZoneScoped;
                ZoneName(pass.debugName.c_str(), pass.debugName.size());

                {
                    ZoneScopedN("Barriers");

                    VkDependencyInfo barriers = {
                        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                        .pNext = nullptr,
                        .memoryBarrierCount = static_cast<u32>(node.memoryBarriers.size()),
                        .pMemoryBarriers = node.memoryBarriers.data(),
                        .bufferMemoryBarrierCount = static_cast<u32>(node.bufferBarriers.size()),
                        .pBufferMemoryBarriers = node.bufferBarriers.data(),
                        .imageMemoryBarrierCount = static_cast<u32>(node.imageBarriers.size()),
                        .pImageMemoryBarriers = node.imageBarriers.data(),
                    };
                    vkCmdPipelineBarrier2(cmd, &barriers);
                }

                if (pass.pipeline)
                {
                    ZoneScopedN("Render using pipeline");
                    if (pass.beginRendering)
                    {
                        ZoneScopedN("Begin rendering");
                        (*pass.beginRendering)(cmd, graph);
                    }

                    vkCmdBindPipeline(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipeline);

                    // TEMP: each renderpass should specify this themselves
                    // if (node.pass.debugName != "CSM pass" && node.pass.debugName != "Tiled light culling pass")
                    if (node.pass.debugName != "CSM pass")
                    {
                        vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 0,
                            1, &sceneDescriptorSet, 0, nullptr);
                    }

                    vkCmdSetViewport(cmd, 0, 1, &viewport);
                    vkCmdSetScissor(cmd, 0, 1, &scissor);

                    {
                        ZoneScopedN("Draw");
                        pass.draw(cmd, graph, pass, scene);
                    }

                    if (pass.beginRendering)
                    {
                        ZoneScopedN("End rendering");
                        vkCmdEndRendering(cmd);
                    }
                }
                else
                {
                    ZoneScopedN("Draw without pipeline");
                    pass.draw(cmd, graph, pass, scene);
                }
            }
        }

        VkExtent2D backbufferSize{backbufferImage.extent.width, backbufferImage.extent.height};
        {
            ZoneScopedCpuGpuAuto("Blit to swapchain", frameCtx);

            vkutil::image::transitionImage(cmd, backbufferImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            vkutil::image::transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkutil::image::blitImageToImage(
                cmd, backbufferImage.image, backbufferSize, swapchainImages[swapchainImageIndex], swapchainSize);
        }

        {
            ZoneScopedCpuGpuAuto("Render Imgui", frameCtx);

            ImGui::Render();

            vkutil::image::transitionImage(cmd, swapchainImages[swapchainImageIndex],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(
                swapchainImageViews[swapchainImageIndex], nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(
                swapchainSize, &colorAttachmentInfo, 1, nullptr);

            vkCmdBeginRendering(cmd, &renderingInfo);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
            vkCmdEndRendering(cmd);
        }

        vkutil::image::transitionImage(cmd, swapchainImages[swapchainImageIndex],
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VK_CHECK(vkEndCommandBuffer(cmd));
    }

    {
        ZoneScopedCpuGpuAuto("Submit Graphics", frameCtx);

        auto cmdInfo = vkutil::init::commandBufferSubmitInfo(cmd);
        auto waitInfo = vkutil::init::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frameCtx.presentSem);
        auto signalInfo = vkutil::init::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frameCtx.renderSem);
        auto submit = vkutil::init::submitInfo2(&cmdInfo, &waitInfo, &signalInfo);

        VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, frameCtx.renderFence));
    }

    {
        ZoneScopedCpuGpuAuto("Present", frameCtx);

        auto presentInfo = vkutil::init::presentInfo(&swapchain, &frameCtx.renderSem, &swapchainImageIndex);
        VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));
    }

    {
        ZoneScopedCpuGpuAuto("Tracy", frameCtx);
        TracyVkCollect(frameCtx.tracyCtx, frameCtx.tracyCmdBuffer);
        VK_CHECK(vkEndCommandBuffer(frameCtx.tracyCmdBuffer));
        auto cmdInfo = vkutil::init::commandBufferSubmitInfo(frameCtx.tracyCmdBuffer);
        auto submit = vkutil::init::submitInfo2(&cmdInfo, nullptr, nullptr);
        VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, frameCtx.tracyRenderFence));
    }
}

auto VulkanBackend::immediateSubmit(std::function<void(VkCommandBuffer)>&& f) -> void
{
    ZoneScopedCpuGpuAuto("Immediate submit", currentFrame());

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

auto VulkanBackend::copyBuffer(VkBuffer src, VkBuffer dst, VkBufferCopy copyRegion) -> void
{
    ZoneScopedCpuGpuAuto("Copy buffer", currentFrame());
    immediateSubmit([&](VkCommandBuffer cmd) { vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion); });
}

auto VulkanBackend::copyBufferWithStaging(void* data, size_t size, VkBuffer dst, VkBufferCopy copyRegion) -> void
{
    ZoneScopedCpuGpuAuto("Copy buffer with staging", currentFrame());

    auto bufInfo = vkutil::init::bufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    AllocatedBuffer staging = allocateBuffer(bufInfo, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VmaAllocationInfo stagingInfo;
    vmaGetAllocationInfo(allocator, staging.allocation, &stagingInfo);

    void* stagingData = stagingInfo.pMappedData;
    memcpy(stagingData, data, size);

    copyRegion.size = size;
    copyBuffer(staging.buffer, dst, copyRegion);

    // TODO: release staging data
}

auto VulkanBackend::allocateBuffer(VkBufferCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags,
    VkMemoryPropertyFlags requiredFlags) -> AllocatedBuffer
{
    AllocatedBuffer buffer;

    VmaAllocationCreateInfo allocInfo{
        .flags = flags,
        .usage = usage,
        .requiredFlags = requiredFlags,
    };
    VK_CHECK(vmaCreateBuffer(allocator, &info, &allocInfo, &buffer.buffer, &buffer.allocation, nullptr));

    return buffer;
}

auto VulkanBackend::allocateImage(VkImageCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags,
    VkMemoryPropertyFlags requiredFlags, VkImageAspectFlags aspectFlags) -> AllocatedImage
{
    AllocatedImage image;
    image.format = info.format;
    image.extent = info.extent;

    VmaAllocationCreateInfo allocInfo{
        .flags = flags,
        .usage = usage,
        .requiredFlags = requiredFlags,
    };
    VK_CHECK(vmaCreateImage(allocator, &info, &allocInfo, &image.image, &image.allocation, nullptr));

    auto imgViewInfo = vkutil::init::imageViewCreateInfo(image.format, image.image, aspectFlags);
    VK_CHECK(vkCreateImageView(device, &imgViewInfo, nullptr, &image.view));

    return image;
}

auto VulkanBackend::getBufferDeviceAddress(VkBuffer buffer) -> VkDeviceAddress
{
    VkBufferDeviceAddressInfo addressInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};

    return vkGetBufferDeviceAddress(device, &addressInfo);
}
