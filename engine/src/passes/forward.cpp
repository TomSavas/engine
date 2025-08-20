#include "passes/forward.h"

#include "engine.h"

#include "GLFW/glfw3.h"
#include "imgui.h"
#include "renderGraph.h"
#include "rhi/renderpass.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"
#include "scene.h"

#include <vulkan/vulkan_core.h>

struct ForwardPushConstants
{
    VkDeviceAddress vertexBufferAddr;
    VkDeviceAddress perModelDataBufferAddr;
    VkDeviceAddress shadowData;
    VkDeviceAddress lightList;
    VkDeviceAddress lightIndexList;
    VkDeviceAddress lightGrid;
    u32 shadowMapIndex;
};

auto initForwardOpaque(VulkanBackend& backend) -> std::optional<ForwardOpaqueRenderer>
{
    return ForwardOpaqueRenderer{
        .pipeline = PipelineBuilder(backend)
            .addDescriptorLayouts({
                backend.sceneDescriptorSetLayout,
                backend.bindlessResources->bindlessTexDescLayout
            })
            .addPushConstants({
                VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .offset = 0,
                    .size = sizeof(ForwardPushConstants)
                }
            })
            .addShader(SHADER_PATH("mesh.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(SHADER_PATH("mesh.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
            .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .polyMode(VK_POLYGON_MODE_FILL)
            .cullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .disableMultisampling()
            .enableAlphaBlending()
            .colorAttachmentFormat(backend.backbufferImage.format)
            .depthFormat(VK_FORMAT_D32_SFLOAT) // TEMP: this should be taken from bindless
            .addViewportScissorDynamicStates()
            .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
            .build(),
    };
}

auto opaqueForwardPass(std::optional<ForwardOpaqueRenderer>& forwardOpaqueRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<Buffer> culledDraws, RenderGraphResource<BindlessTexture> depthMap,
    RenderGraphResource<Buffer> shadowData, RenderGraphResource<BindlessTexture> shadowMap,
    LightData lightData)
    -> void
{
    if (!forwardOpaqueRenderer)
    {
        forwardOpaqueRenderer = initForwardOpaque(backend);
    }

    auto& pass = createPass(graph);
    pass.pass.debugName = "Forward Opaque pass";
    pass.pass.pipeline = forwardOpaqueRenderer->pipeline;

    struct ForwardOpaqueRenderGraphData
    {
        RenderGraphResource<Buffer> culledDraws;
        RenderGraphResource<Buffer> shadowData;
        RenderGraphResource<BindlessTexture> shadowMap;
        RenderGraphResource<BindlessTexture> depthMap;
        RenderGraphResource<Buffer> lightList;
        RenderGraphResource<Buffer> lightIndexList;
        RenderGraphResource<Buffer> lightGrid;
    } data = {
        .culledDraws = readResource<Buffer>(graph, pass, culledDraws),
        .shadowData = readResource<Buffer>(graph, pass, shadowData),
        .shadowMap = readResource<BindlessTexture>(graph, pass, shadowMap, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL),
        .depthMap = readResource<BindlessTexture>(graph, pass, depthMap, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL),
        .lightList = readResource<Buffer>(graph, pass, lightData.lightList),
        .lightIndexList = readResource<Buffer>(graph, pass, lightData.lightIndexList),
        .lightGrid = readResource<Buffer>(graph, pass, lightData.lightGrid)
    };

    pass.pass.beginRendering = [data, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    {
        const VkExtent2D swapchainSize = {
            static_cast<u32>(backend.viewport.width),
            static_cast<u32>(backend.viewport.height)
        };
        VkClearValue colorClear = {
            .color = {.uint32 = {0, 0, 0, 0}}
        };
        auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(
            backend.backbufferImage.view, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(
            backend.bindlessResources->getTexture(
                *getResource<BindlessTexture>(graph, data.depthMap)).view,
                // TEMPORARY
                VK_ATTACHMENT_LOAD_OP_CLEAR);
                //VK_ATTACHMENT_LOAD_OP_LOAD);
        auto renderingInfo = vkutil::init::renderingInfo(swapchainSize, &colorAttachmentInfo, 1, &depthAttachmentInfo);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [data, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene)
    {
        ZoneScopedCpuGpuAuto("Forward opaque pass", backend.currentFrame());

        const ForwardPushConstants pushConstants = {
            .vertexBufferAddr = backend.getBufferDeviceAddress(scene.vertexBuffer.buffer),
            .perModelDataBufferAddr = backend.getBufferDeviceAddress(scene.perModelBuffer.buffer),
            .shadowData = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.shadowData)),
            .lightList = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.lightList)),
            .lightIndexList = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.lightIndexList)),
            .lightGrid = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.lightGrid)),
            .shadowMapIndex = *getResource<BindlessTexture>(graph, data.shadowMap)
        };
        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
            &pushConstants);
        vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdBindIndexBuffer(cmd, scene.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, *getResource<Buffer>(graph, data.culledDraws), 0, scene.meshes.size(),
            sizeof(VkDrawIndexedIndirectCommand));
    };
}
