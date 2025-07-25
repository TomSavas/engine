#include "passes/lightCulling.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/shader.h"
#include "rhi/vulkan/utils/inits.h"
#include "scene.h"

struct PushConstants
{
    uint depthMap;
    VkDeviceAddress lightList;
    VkDeviceAddress lightIndexList;
    VkDeviceAddress lightGrid;
    VkDeviceAddress lightCount;
};

std::optional<LightCulling> initLightCulling(VulkanBackend& backend, Scene& scene, const uint16_t tileCount[2])
{
    const uint16_t lightGridSize = tileCount[0] * tileCount[1];

    constexpr uint16_t maxLightsPerTile = 1048;
    constexpr uint16_t maxLights = 20000;
    constexpr VkBufferUsageFlags lightBufferFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    return LightCulling{
        .pipeline = PipelineBuilder(backend)
            .addDescriptorLayouts({
                backend.sceneDescriptorSetLayout,
                backend.bindlessResources->bindlessTexDescLayout
            })
            .addPushConstants({
                VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    .offset = 0,
                    .size = sizeof(PushConstants)
                }
            })
            .addShader(SHADER_PATH("tiledLightCulling.comp.glsl"), VK_SHADER_STAGE_COMPUTE_BIT)
            .build(),
        .lightList = backend.allocateBuffer(
            vkutil::init::bufferCreateInfo(sizeof(decltype(scene.pointLights)::value_type) * maxLights,
                lightBufferFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .lightIndexList = backend.allocateBuffer(
            vkutil::init::bufferCreateInfo(sizeof(uint64_t) * lightGridSize * maxLightsPerTile, lightBufferFlags),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .lightGrid = backend.allocateBuffer(
            vkutil::init::bufferCreateInfo(sizeof(uint64_t) * 2 * 2 * lightGridSize, lightBufferFlags),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        // TEMP:
        .lightCount = backend.allocateBuffer(
            vkutil::init::bufferCreateInfo(sizeof(uint64_t), lightBufferFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
}

LightData tiledLightCullingPass(std::optional<LightCulling>& lightCulling, VulkanBackend& backend, RenderGraph& graph,
    Scene& scene, RenderGraphResource<BindlessTexture> depthMap, float tileSizeAsPercentageOfScreen)
{
    // TODO: this should be dynamic
    const uint16_t tileCount[2] = {
        static_cast<uint16_t>(std::ceil(static_cast<float>(backend.backbufferImage.extent.width) * tileSizeAsPercentageOfScreen)),
        static_cast<uint16_t>(std::ceil(static_cast<float>(backend.backbufferImage.extent.height) * tileSizeAsPercentageOfScreen))
    };
    std::println("!!!!!!!!!!!!! Using {}x{} tiles for light culling.", tileCount[0], tileCount[1]);
    if (!lightCulling)
    {
        lightCulling = initLightCulling(backend, scene, tileCount);
    }

    RenderGraph::Node& pass = createPass(graph);
    pass.pass.debugName = "Tiled light culling pass";
    pass.pass.pipeline = lightCulling->pipeline;

    LightData data = {
        // TODO: light list should be uploaded in a separate, earlier pass
        .depthMap = readResource<BindlessTexture>(graph, pass, depthMap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        .lightList = readResource<Buffer>(
            graph, pass, importResource<Buffer>(graph, pass, &lightCulling->lightList.buffer)),
        .lightIndexList = writeResource<Buffer>(
            graph, pass, importResource<Buffer>(graph, pass, &lightCulling->lightIndexList.buffer)),
        .lightGrid = writeResource<Buffer>(
            graph, pass, importResource<Buffer>(graph, pass, &lightCulling->lightGrid.buffer)),
        // TEMP:
        .lightCount = writeResource<Buffer>(
            graph, pass, importResource<Buffer>(graph, pass, &lightCulling->lightCount.buffer)),
    };

    // pass.pass.beginRendering = [data, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    // {
    //     // // TODO: attach depth from Z prepass
    //     // VkExtent2D swapchainSize = {
    //     //     static_cast<uint32_t>(backend.viewport.width),
    //     //     static_cast<uint32_t>(backend.viewport.height)
    //     // };
    //     // VkClearValue colorClear = {
    //     //     .color = {.uint32 = {0, 0, 0, 0}}
    //     // };
    //     // VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(
    //     //     backend.backbufferImage.view, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    //     // VkRenderingAttachmentInfo depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(
    //     //     backend.bindlessResources->getTexture(
    //     //         *getResource<BindlessTexture>(graph, data.depthMap)).view,
    //     //         VK_ATTACHMENT_LOAD_OP_LOAD);
    //     // VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(
    //     //     swapchainSize, &colorAttachmentInfo, 1, &depthAttachmentInfo);
    //     // vkCmdBeginRendering(cmd, &renderingInfo);
    // };

    pass.pass.draw = [data, tileCount, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene)
    {
        ZoneScopedCpuGpuAuto("Tiled light culling pass", backend.currentFrame());

        // TODO: this should live in a separate pass at the start, for uploading all the scene info.
        uint32_t count = scene.pointLights.size();
        backend.copyBufferWithStaging(&count, sizeof(count), *getResource<Buffer>(graph, data.lightList));
        backend.copyBufferWithStaging(scene.pointLights.data(),
            sizeof(decltype(scene.pointLights)::value_type) * scene.pointLights.size(),
            *getResource<Buffer>(graph, data.lightList),
            VkBufferCopy{
                .srcOffset = 0,
                .dstOffset = sizeof(glm::vec4),
            }
        );

        // TEMP:
        count = 0;
        backend.copyBufferWithStaging(&count, sizeof(count), *getResource<Buffer>(graph, data.lightCount));

        const PushConstants pushConstants = {
            .depthMap = *getResource<BindlessTexture>(graph, data.depthMap),
            .lightList = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.lightList)),
            .lightIndexList = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.lightIndexList)),
            .lightGrid = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.lightGrid)),
            // TEMP:
            .lightCount = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.lightCount)),
        };

        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants),
            &pushConstants);
        vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdDispatch(cmd, tileCount[0], tileCount[1], 1);
    };

    return data;
}

LightData clusteredLightCullingPass(std::optional<LightCulling>& lightCulling, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> depthMap, Scene& scene)
{
    return tiledLightCullingPass(lightCulling, backend, graph, scene, depthMap, 1.f / 32.f);
}
