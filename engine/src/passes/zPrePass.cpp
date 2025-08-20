#include "passes/zPrePass.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/utils/inits.h"
#include "scene.h"

struct ZPrePassPushConstants
{
    VkDeviceAddress vertexBufferAddr;
    VkDeviceAddress perModelDataBufferAddr;
};

auto initZPrePass(VulkanBackend& backend) -> std::optional<ZPrePassRenderer>
{
    const auto depthImage = backend.allocateImage(vkutil::init::imageCreateInfo(VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        backend.backbufferImage.extent, 1), VMA_MEMORY_USAGE_GPU_ONLY, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    return ZPrePassRenderer{
        .pipeline = PipelineBuilder(backend)
            .addDescriptorLayouts({
                backend.sceneDescriptorSetLayout,
                backend.bindlessResources->bindlessTexDescLayout
            })
            .addPushConstants({
                VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .offset = 0,
                    .size = sizeof(ZPrePassPushConstants)
                }
            })
            .addShader(SHADER_PATH("zPrePass.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(SHADER_PATH("empty.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
            .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .polyMode(VK_POLYGON_MODE_FILL)
            .cullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .disableMultisampling()
            .enableAlphaBlending()
            .depthFormat(depthImage.format)
            .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
            .addViewportScissorDynamicStates()
            .build(),
        .depthMap = backend.bindlessResources->addTexture(
            Texture{
                .image = depthImage,
                .view = depthImage.view,
                .mipCount = 1,
            }
        ),
    };
}

auto zPrePass(std::optional<ZPrePassRenderer>& renderer, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<Buffer> culledDraws)
    -> ZPrePassRenderGraphData
{
    if (!renderer)
    {
        renderer = initZPrePass(backend);
    }

    auto& pass = createPass(graph);
    pass.pass.debugName = "Z Pre pass";
    pass.pass.pipeline = renderer->pipeline;

    ZPrePassRenderGraphData data = {
        .depthMap = readResource<BindlessTexture>(graph, pass,
            importResource(graph, pass, &renderer->depthMap), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL),
    };
    culledDraws = readResource<Buffer>(graph, pass, culledDraws);

    pass.pass.beginRendering = [data, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    {
        const VkExtent2D swapchainSize = {
            static_cast<u32>(backend.viewport.width),
            static_cast<u32>(backend.viewport.height)
        };
        const auto& depthMap = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(graph,
            data.depthMap));
        auto depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(depthMap.view);
        const auto renderingInfo = vkutil::init::renderingInfo(swapchainSize, nullptr, 0, &depthAttachmentInfo);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [culledDraws, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass,
        Scene& scene)
    {
        ZoneScopedCpuGpuAuto("Z Pre pass", backend.currentFrame());

        const ZPrePassPushConstants pushConstants = {
            .vertexBufferAddr = backend.getBufferDeviceAddress(scene.vertexBuffer.buffer),
            .perModelDataBufferAddr = backend.getBufferDeviceAddress(scene.perModelBuffer.buffer),
        };
        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
            &pushConstants);
        vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdBindIndexBuffer(cmd, scene.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, *getResource<Buffer>(graph, culledDraws), 0, scene.meshes.size(),
            sizeof(VkDrawIndexedIndirectCommand));
    };

    return data;
}
