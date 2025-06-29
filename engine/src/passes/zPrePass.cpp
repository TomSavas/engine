#include "zPrePass.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/utils/inits.h"
#include "scene.h"

struct PushConstants
{
    VkDeviceAddress vertexBufferAddr;
    VkDeviceAddress perModelDataBufferAddr;
};

std::optional<ZPrePassRenderer> initZPrePass(VulkanBackend& backend)
{
    ZPrePassRenderer renderer;

    // Depth map
    AllocatedImage depthImage = backend.allocateImage(vkutil::init::imageCreateInfo(VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        backend.backbufferImage.extent, 1), VMA_MEMORY_USAGE_GPU_ONLY, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);
    Texture depthMap = {
        .image = depthImage,
        .view = depthImage.view,
        .mipCount = 1,
    };
    renderer.depthMap = backend.bindlessResources->addTexture(depthMap);

    std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(
        backend.device, SHADER_PATH("zPrePass.vert.glsl"));
    std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(
        backend.device, SHADER_PATH("empty.frag.glsl"));
    if (!vertexShader || !fragmentShader)
    {
        return std::nullopt;
    }

    VkPushConstantRange meshPushConstantRange = vkutil::init::pushConstantRange(
        VK_SHADER_STAGE_ALL, sizeof(PushConstants));
    VkDescriptorSetLayout descriptors[] = {
        backend.sceneDescriptorSetLayout, backend.bindlessResources->bindlessTexDescLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(
        descriptors, 2, &meshPushConstantRange, 1);
    VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &renderer.pipeline.pipelineLayout));

    renderer.pipeline.pipeline = PipelineBuilder()
                                     .shaders((*vertexShader)->module, (*fragmentShader)->module)
                                     .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                     .polyMode(VK_POLYGON_MODE_FILL)
                                     .cullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                                     .disableMultisampling()
                                     .enableAlphaBlending()
                                     .depthFormat(depthImage.format)
                                     .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                                     .addViewportScissorDynamicStates()
                                     .build(backend.device, renderer.pipeline.pipelineLayout);
    renderer.pipeline.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    return renderer;
}

ZPrePassRenderGraphData zPrePass(std::optional<ZPrePassRenderer>& renderer, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<Buffer> culledDraws)
{
    if (!renderer)
    {
        renderer = initZPrePass(backend);
    }

    RenderGraph::Node& pass = createPass(graph);
    pass.pass.debugName = "Z Pre pass";
    pass.pass.pipeline = renderer->pipeline;

    ZPrePassRenderGraphData data = {
        .depthMap = readResource<BindlessTexture>(graph, pass,
            importResource<BindlessTexture>(graph, pass, &renderer->depthMap),
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL),
    };
    culledDraws = readResource<Buffer>(graph, pass, culledDraws);

    pass.pass.beginRendering = [data, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    {
        const VkExtent2D swapchainSize = {
            static_cast<uint32_t>(backend.viewport.width),
            static_cast<uint32_t>(backend.viewport.height)
        };
        const Texture& depthMap = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(graph,
            data.depthMap));
        VkRenderingAttachmentInfo depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(depthMap.view);
        VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(swapchainSize, nullptr, 0, &depthAttachmentInfo);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [culledDraws, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass,
        Scene& scene)
    {
        ZoneScopedCpuGpuAuto("Z Pre pass", backend.currentFrame());

        PushConstants pushConstants = {
            .vertexBufferAddr = backend.getBufferDeviceAddress(scene.vertexBuffer.buffer),
            .perModelDataBufferAddr = backend.getBufferDeviceAddress(scene.perModelBuffer.buffer),
        };
        vkCmdPushConstants(
            cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pushConstants);
        vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdBindIndexBuffer(cmd, scene.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, *getResource<Buffer>(graph, culledDraws), 0, scene.meshes.size(),
            sizeof(VkDrawIndexedIndirectCommand));
    };

    return data;
}
