#include "passes/lightCulling.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/shader.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"
#include "scene.h"

struct LightCullingPushConstants
{
    u32 depthMap;
    VkDeviceAddress lightList;
    VkDeviceAddress lightIndexList;
    VkDeviceAddress lightGrid;
    VkDeviceAddress lightCount;
};

auto initLightCulling(VulkanBackend& backend, Scene& scene, const u16 tileCount[2]) -> LightCulling
{
    const u16 lightGridSize = tileCount[0] * tileCount[1];

    constexpr u16 maxLightsPerTile = 1048;
    constexpr u16 maxLights = 20000;
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
                    .size = sizeof(LightCullingPushConstants)
                }
            })
            .addShader(SHADER_PATH("tiledLightCulling.comp.glsl"), VK_SHADER_STAGE_COMPUTE_BIT)
            .build(),
        .lightList = backend.allocateBuffer(
            "Light list",
            vkutil::init::bufferCreateInfo(sizeof(decltype(scene.pointLights)::value_type) * maxLights,
                lightBufferFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .lightIndexList = backend.allocateBuffer(
            "Light index list",
            vkutil::init::bufferCreateInfo(sizeof(u64) * lightGridSize * maxLightsPerTile, lightBufferFlags),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .lightGrid = backend.allocateBuffer(
            "Light grid",
            vkutil::init::bufferCreateInfo(sizeof(u64) * 2 * 2 * lightGridSize, lightBufferFlags),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        // TEMP:
        .lightCount = backend.allocateBuffer(
            "Light count",
            vkutil::init::bufferCreateInfo(sizeof(u64), lightBufferFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
}

auto tiledLightCullingPass(std::optional<LightCulling>& lightCulling, VulkanBackend& backend, RenderGraph& graph,
    Scene& scene, RenderGraphResource<BindlessTexture> depthMap, f32 tileSizeAsPercentageOfScreen)
    -> LightData
{
    // TODO: This should inspect some GPU capabilities
    const u16 tileCount[2] = {
        static_cast<u16>(std::ceil(static_cast<f32>(backend.scaledResolution.x) * tileSizeAsPercentageOfScreen)),
        static_cast<u16>(std::ceil(static_cast<f32>(backend.scaledResolution.y) * tileSizeAsPercentageOfScreen))
    };
    // std::println("Using {}x{} tiles for light culling.", tileCount[0], tileCount[1]);

    if (!lightCulling)
    {
        lightCulling = initLightCulling(backend, scene, tileCount);
    }

    auto& pass = createPass(graph);
    pass.pass.debugName = "Tiled light culling pass";
    pass.pass.pipeline = lightCulling->pipeline;

    LightData data = {
        // TODO: light list should be uploaded in a separate, earlier pass
        .depthMap = readResource<BindlessTexture>(graph, pass, depthMap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        .lightList = readResource<Buffer>(graph, pass, importResource(graph, pass, &lightCulling->lightList)),
        .lightIndexList = writeResource<Buffer>(
            graph, pass, importResource(graph, pass, &lightCulling->lightIndexList.buffer)),
        .lightGrid = writeResource<Buffer>(graph, pass, importResource(graph, pass, &lightCulling->lightGrid)),
        // TEMP:
        .lightCount = writeResource<Buffer>(graph, pass, importResource(graph, pass, &lightCulling->lightCount)),
    };

    pass.pass.prepare = [&backend, data](const RenderContext& ctx)
    {
        ZoneScopedCpuGpuAuto("Tiled light culling pass", backend.currentFrame());

        // TODO: this should live in a separate pass at the start, for uploading all the scene info.
        u32 count = ctx.scene.pointLights.size();
        backend.copyBufferWithStaging(ctx.cmd, &count, sizeof(count), getResource<Buffer>(ctx.graph, data.lightList)->buffer);
        backend.copyBufferWithStaging(ctx.cmd, ctx.scene.pointLights.data(),
            sizeof(decltype(ctx.scene.pointLights)::value_type) * ctx.scene.pointLights.size(),
            getResource<Buffer>(ctx.graph, data.lightList)->buffer,
            VkBufferCopy{
                .srcOffset = 0,
                .dstOffset = sizeof(glm::vec4),
            }
        );

        // TEMP:
        count = 0;
        backend.copyBufferWithStaging(ctx.cmd, &count, sizeof(count), getResource<Buffer>(ctx.graph, data.lightCount)->buffer);
    };

    pass.pass.draw = [data, tileCount, &backend](const RenderContext& ctx, RenderPass& pass)
    {
        ZoneScopedCpuGpuAuto("Tiled light culling pass", backend.currentFrame());

        const LightCullingPushConstants pushConstants = {
            .depthMap = *getResource<BindlessTexture>(ctx.graph, data.depthMap),
            .lightList = backend.getBufferDeviceAddress(getResource<Buffer>(ctx.graph, data.lightList)->buffer),
            .lightIndexList = backend.getBufferDeviceAddress(getResource<Buffer>(ctx.graph, data.lightIndexList)->buffer),
            .lightGrid = backend.getBufferDeviceAddress(getResource<Buffer>(ctx.graph, data.lightGrid)->buffer),
            // TEMP:
            .lightCount = backend.getBufferDeviceAddress(getResource<Buffer>(ctx.graph, data.lightCount)->buffer),
        };

        vkCmdPushConstants(ctx.cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants),
            &pushConstants);
        vkCmdBindDescriptorSets(ctx.cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdDispatch(ctx.cmd, tileCount[0], tileCount[1], 1);
    };

    return data;
}

auto clusteredLightCullingPass(std::optional<LightCulling>& lightCulling, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> depthMap, Scene& scene)
    -> LightData
{
    return tiledLightCullingPass(lightCulling, backend, graph, scene, depthMap, 1.f / 32.f);
}
