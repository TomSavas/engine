#include "passes/lightCulling.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/shader.h"
#include "rhi/vulkan/utils/inits.h"

std::optional<LightCulling> initLightCulling(VulkanBackend& backend)
{
    //LightCulling lightCulling;

    //std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(
    //    backend.device, SHADER_PATH("mesh.vert.glsl"));
    //std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(
    //    backend.device, SHADER_PATH("mesh.frag.glsl"));
    //if (!vertexShader || !fragmentShader)
    //{
    //    return std::nullopt;
    //}

    //// VkPushConstantRange meshPushConstantRange = vkutil::init::pushConstantRange(
    //    // VK_SHADER_STAGE_ALL, sizeof(PushConstants));
    //VkDescriptorSetLayout descriptors[] = {
    //    backend.sceneDescriptorSetLayout, backend.bindlessResources->bindlessTexDescLayout};
    //VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(
    //    // descriptors, 2, &meshPushConstantRange, 1);
    //    descriptors, 2, nullptr, 0);
    //VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &lightCulling.pipeline.pipelineLayout));

    //lightCulling.pipeline.pipeline = PipelineBuilder(backend)
    //                                 .shaders((*vertexShader)->module, (*fragmentShader)->module)
    //                                 .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    //                                 .polyMode(VK_POLYGON_MODE_FILL)
    //                                 .cullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
    //                                 .disableMultisampling()
    //                                 .enableAlphaBlending()
    //                                 .colorAttachmentFormat(backend.backbufferImage.format)
    //                                 .depthFormat(VK_FORMAT_D32_SFLOAT) // TEMP: this should be taken from bindless
    //                                 .addViewportScissorDynamicStates()
    //                                 .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
    //                                 .build(backend.device, lightCulling.pipeline.pipelineLayout);
    //lightCulling.pipeline.pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

    //return lightCulling;

    constexpr VkBufferUsageFlags lightBufferFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    return LightCulling {
        .pipeline = PipelineBuilder(backend)
            .addDescriptorLayouts({})
            .addPushConstants({})
            .addShader(SHADER_PATH("tiledLightCulling.comp.glsl"), VK_SHADER_STAGE_COMPUTE_BIT)
            .build(),
        .lightList = backend.allocateBuffer(
            vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * 1000, lightBufferFlags),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .lightIndexList = backend.allocateBuffer(
            vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * 1000, lightBufferFlags),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .lightGrid = backend.allocateBuffer(
            vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * 1000, lightBufferFlags),
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
}

LightData tiledLightCullingPass(std::optional<LightCulling>& lightCulling, VulkanBackend& backend, RenderGraph& graph)
{
    if (!lightCulling)
    {
        lightCulling = initLightCulling(backend);
    }

    RenderGraph::Node& pass = createPass(graph);
    pass.pass.debugName = "Tiled light culling pass";
    pass.pass.pipeline = lightCulling->pipeline;

    LightData data = {
        // TODO: light list should be uploaded in a separate, earlier pass
        .lightList = readResource<Buffer>(graph, pass,
            importResource<Buffer>(graph, pass, &lightCulling->lightList.buffer)),
        .lightIndexList = writeResource<Buffer>(graph, pass,
            importResource<Buffer>(graph, pass, &lightCulling->lightIndexList.buffer)),
        .lightGrid = writeResource<Buffer>(graph, pass,
            importResource<Buffer>(graph, pass, &lightCulling->lightGrid.buffer)),
    };

    pass.pass.beginRendering = [data, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    {
        // // TODO: attach depth from Z prepass
        // VkExtent2D swapchainSize = {
        //     static_cast<uint32_t>(backend.viewport.width),
        //     static_cast<uint32_t>(backend.viewport.height)
        // };
        // VkClearValue colorClear = {
        //     .color = {.uint32 = {0, 0, 0, 0}}
        // };
        // VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(
        //     backend.backbufferImage.view, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // VkRenderingAttachmentInfo depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(
        //     backend.bindlessResources->getTexture(
        //         *getResource<BindlessTexture>(graph, data.depthMap)).view,
        //         VK_ATTACHMENT_LOAD_OP_LOAD);
        // VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(
        //     swapchainSize, &colorAttachmentInfo, 1, &depthAttachmentInfo);
        // vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [data, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene)
    {
        // ZoneScopedCpuGpuAuto("Forward opaque pass", backend.currentFrame());
        //
        // PushConstants pushConstants = {
        //     .vertexBufferAddr = backend.getBufferDeviceAddress(scene.vertexBuffer.buffer),
        //     .perModelDataBufferAddr = backend.getBufferDeviceAddress(scene.perModelBuffer.buffer),
        //     .shadowData = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.shadowData)),
        //     .shadowMapIndex = *getResource<BindlessTexture>(graph, data.shadowMap)
        // };
        // vkCmdPushConstants(
        //     cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pushConstants);
        // vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
        //     &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        // vkCmdBindIndexBuffer(cmd, scene.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        // vkCmdDrawIndexedIndirect(cmd, *getResource<Buffer>(graph, data.culledDraws), 0, scene.meshes.size(),
        //     sizeof(VkDrawIndexedIndirectCommand));
    };

}

LightData clusteredLightCullingPass(std::optional<LightCulling>& lightCulling, VulkanBackend& backend,
    RenderGraph& graph)
{
    return tiledLightCullingPass(lightCulling, backend, graph);
}
