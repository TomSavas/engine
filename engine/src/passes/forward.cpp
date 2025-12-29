#include "passes/forward.h"

#include "engine.h"

#include "GLFW/glfw3.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <vulkan/vulkan_core.h>

#include "debugUI.h"
#include "imgui.h"
#include "renderGraph.h"
#include "rhi/renderpass.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"
#include "scene.h"

struct ForwardPushConstants
{
    glm::vec4 enabledFeatures; // normal mapping, parallax mapping
    VkDeviceAddress vertexBufferAddr;
    VkDeviceAddress perModelDataBufferAddr;
    VkDeviceAddress shadowData;
    VkDeviceAddress lightList;
    VkDeviceAddress lightIndexList;
    VkDeviceAddress lightGrid;
    u32 shadowMapIndex;
    u32 depthMapIndex;
};

auto initForwardOpaque(VulkanBackend& backend) -> ForwardOpaqueRenderer
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
            .colorAttachmentFormat(vkutil::init::kDefaultColorFormat)
            .enableAlphaBlending()
            .colorAttachmentFormat(vkutil::init::kDefaultColorFormat)
            .enableAlphaBlending()
            .colorAttachmentFormat(vkutil::init::kDefaultColorFormat)
            .enableAlphaBlending()
            .colorAttachmentFormat(vkutil::init::kDefaultColorFormat)
            .enableAlphaBlending()
            .depthFormat(VK_FORMAT_D32_SFLOAT) // TEMP: this should be taken from bindless
            .addViewportScissorDynamicStates()
            .enableDepthTest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
            .build(),
        .color = backend.bindlessResources->addTexture(
            backend.allocateTexture(
                "Forward color output",
                vkutil::init::defaultColorAttachmentTextureCreateInfo(backend.scaledResolution),
                vkutil::init::defaultTextureAllocationCreateInfo(),
                MipOptions::one(),
                VK_IMAGE_ASPECT_COLOR_BIT
            )
        ),
        .normal = backend.bindlessResources->addTexture(
            backend.allocateTexture(
                "Forward normal output",
                vkutil::init::defaultColorAttachmentTextureCreateInfo(backend.scaledResolution),
                vkutil::init::defaultTextureAllocationCreateInfo(),
                MipOptions::one(),
                VK_IMAGE_ASPECT_COLOR_BIT
            )
        ),
        .positions = backend.bindlessResources->addTexture(
            backend.allocateTexture(
                "Forward position output",
                vkutil::init::defaultColorAttachmentTextureCreateInfo(backend.scaledResolution),
                vkutil::init::defaultTextureAllocationCreateInfo(),
                MipOptions::one(),
                VK_IMAGE_ASPECT_COLOR_BIT
            )
        ),
        .reflections = backend.bindlessResources->addTexture(
            backend.allocateTexture(
                "Forward SSR reflection output",
                vkutil::init::defaultColorAttachmentTextureCreateInfo(backend.scaledResolution),
                vkutil::init::defaultTextureAllocationCreateInfo(),
                MipOptions::one(),
                VK_IMAGE_ASPECT_COLOR_BIT
            )
        ),
    };
}

auto opaqueForwardPass(std::optional<ForwardOpaqueRenderer>& forwardOpaqueRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<Buffer> culledDraws, RenderGraphResource<BindlessTexture> depthMap,
    RenderGraphResource<Buffer> shadowData, RenderGraphResource<BindlessTexture> shadowMap,
    LightData lightData, RenderGraphResource<Buffer> perModelData)
    -> ForwardRenderGraphData
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
        RenderGraphResource<BindlessTexture> color;
        RenderGraphResource<BindlessTexture> normal;
        RenderGraphResource<BindlessTexture> positions;
        RenderGraphResource<BindlessTexture> reflections;
        RenderGraphResource<Buffer> perModelData;
    } data = {
        .culledDraws = readResource<Buffer>(graph, pass, culledDraws),
        .shadowData = readResource<Buffer>(graph, pass, shadowData),
        .shadowMap = readResource<BindlessTexture>(graph, pass, shadowMap, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL),
        .depthMap = readResource<BindlessTexture>(graph, pass, depthMap, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL),
        .lightList = readResource<Buffer>(graph, pass, lightData.lightList),
        .lightIndexList = readResource<Buffer>(graph, pass, lightData.lightIndexList),
        .lightGrid = readResource<Buffer>(graph, pass, lightData.lightGrid),
        .color = writeResource<BindlessTexture>(graph, pass,
            importResource<BindlessTexture>(graph, pass, &forwardOpaqueRenderer->color),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        .normal = writeResource<BindlessTexture>(graph, pass,
            importResource<BindlessTexture>(graph, pass, &forwardOpaqueRenderer->normal),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        .positions = writeResource<BindlessTexture>(graph, pass,
            importResource<BindlessTexture>(graph, pass, &forwardOpaqueRenderer->positions),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        .reflections = writeResource<BindlessTexture>(graph, pass,
            importResource<BindlessTexture>(graph, pass, &forwardOpaqueRenderer->reflections),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        .perModelData = readResource<Buffer>(graph, pass, perModelData),
    };

    pass.pass.beginRendering = [data, &backend](const RenderContext& ctx)
    {
        VkClearValue colorClear = {
            .color = {.uint32 = {0, 0, 0, 0}}
        };
        const auto& colorImage = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(ctx.graph,
            data.color));
        auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(colorImage.image.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const auto& normalImage = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(ctx.graph,
            data.normal));
        auto normalAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(normalImage.image.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const auto& positionImage = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(ctx.graph,
            data.positions));
        auto positionAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(positionImage.image.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const auto& reflectionImage = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(ctx.graph,
            data.reflections));
        auto reflectionAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(reflectionImage.image.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo attachments[] = {
            colorAttachmentInfo,
            normalAttachmentInfo,
            positionAttachmentInfo,
            reflectionAttachmentInfo,
        };
        auto depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(
            backend.bindlessResources->getTexture(
                *getResource<BindlessTexture>(ctx.graph, data.depthMap)).image.view,
                // No clear -- we're using ZPrePass
                VK_ATTACHMENT_LOAD_OP_LOAD,
                VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
        auto renderingInfo = vkutil::init::renderingInfo(ctx.swapchain.size, attachments, std::size(attachments),
            &depthAttachmentInfo);
        vkCmdBeginRendering(ctx.cmd, &renderingInfo);
    };

    pass.pass.draw = [data, &backend](const RenderContext& ctx, RenderPass& pass) -> void
    {
        ZoneScopedCpuGpuAuto("Forward opaque pass", backend.currentFrame());

        static bool normalMappingEnabled = true;
        static bool parallaxMappingEnabled = true;
        addDebugUI(debugUI, GRAPHICS_PASSES, [&]()
        {
            if (ImGui::TreeNode("Forward Opaque"))
            {
                ImGui::Checkbox("Normal mapping", &normalMappingEnabled);
                ImGui::Checkbox("Parallax mapping", &parallaxMappingEnabled);

                ImGui::TreePop();
            }
        });

        const ForwardPushConstants pushConstants = {
            .enabledFeatures = glm::vec4(
                normalMappingEnabled ? 1.f : 0.f,
                parallaxMappingEnabled ? 1.f : 0.f,
                0.f,
                0.f
            ),
            .vertexBufferAddr = backend.getBufferDeviceAddress(ctx.scene.vertexBuffer.buffer),
            .perModelDataBufferAddr = backend.getBufferDeviceAddress(getResource<Buffer>(ctx.graph, data.perModelData)->buffer),
            .shadowData = backend.getBufferDeviceAddress(getResource<Buffer>(ctx.graph, data.shadowData)->buffer),
            .lightList = backend.getBufferDeviceAddress(getResource<Buffer>(ctx.graph, data.lightList)->buffer),
            .lightIndexList = backend.getBufferDeviceAddress(getResource<Buffer>(ctx.graph, data.lightIndexList)->buffer),
            .lightGrid = backend.getBufferDeviceAddress(getResource<Buffer>(ctx.graph, data.lightGrid)->buffer),
            .shadowMapIndex = *getResource<BindlessTexture>(ctx.graph, data.shadowMap),
            .depthMapIndex = *getResource<BindlessTexture>(ctx.graph, data.depthMap),
        };
        vkCmdPushConstants(ctx.cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
            &pushConstants);
        vkCmdBindDescriptorSets(ctx.cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdBindIndexBuffer(ctx.cmd, ctx.scene.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(ctx.cmd, getResource<Buffer>(ctx.graph, data.culledDraws)->buffer, 0, ctx.scene.meshes.size(),
            sizeof(VkDrawIndexedIndirectCommand));
    };

    return ForwardRenderGraphData {
        .color = data.color,
        .normal = data.normal,
        .positions = data.positions,
        .reflections = data.reflections
    };
}
