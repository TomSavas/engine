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

auto initForwardOpaque(VulkanBackend& backend) -> std::optional<ForwardOpaqueRenderer>
{
    const auto reflectionImage = backend.allocateImage(vkutil::init::imageCreateInfo(VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        backend.backbufferImage.extent, 1), VMA_MEMORY_USAGE_GPU_ONLY, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    const auto outputNormalImage = backend.allocateImage(vkutil::init::imageCreateInfo(VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        backend.backbufferImage.extent, 1), VMA_MEMORY_USAGE_GPU_ONLY, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    const auto outputPositionImage = backend.allocateImage(vkutil::init::imageCreateInfo(VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        backend.backbufferImage.extent, 1), VMA_MEMORY_USAGE_GPU_ONLY, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    const auto outputImage = backend.allocateImage(vkutil::init::imageCreateInfo(VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        backend.backbufferImage.extent, 1), VMA_MEMORY_USAGE_GPU_ONLY, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

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
            .colorAttachmentFormat(outputImage.format)
            .enableAlphaBlending()
            .colorAttachmentFormat(outputNormalImage.format)
            .enableAlphaBlending()
            .colorAttachmentFormat(outputPositionImage.format)
            .enableAlphaBlending()
            .colorAttachmentFormat(reflectionImage.format)
            .enableAlphaBlending()
            .depthFormat(VK_FORMAT_D32_SFLOAT) // TEMP: this should be taken from bindless
            .addViewportScissorDynamicStates()
            .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
            .build(),
        .color = backend.bindlessResources->addTexture(
            Texture{
                .image = outputImage,
                .view = outputImage.view,
                .mipCount = 1,
            }
        ),
        .normal = backend.bindlessResources->addTexture(
            Texture{
                .image = outputNormalImage,
                .view = outputNormalImage.view,
                .mipCount = 1,
            }
        ),
        .positions = backend.bindlessResources->addTexture(
            Texture{
                .image = outputPositionImage,
                .view = outputPositionImage.view,
                .mipCount = 1,
            }
        ),
        .reflections = backend.bindlessResources->addTexture(
            Texture{
                .image = reflectionImage,
                .view = reflectionImage.view,
                .mipCount = 1,
            }
        ),
    };
}

auto opaqueForwardPass(std::optional<ForwardOpaqueRenderer>& forwardOpaqueRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<Buffer> culledDraws, RenderGraphResource<BindlessTexture> depthMap,
    RenderGraphResource<Buffer> shadowData, RenderGraphResource<BindlessTexture> shadowMap,
    LightData lightData)
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
    } data = {
        .culledDraws = readResource<Buffer>(graph, pass, culledDraws),
        .shadowData = readResource<Buffer>(graph, pass, shadowData),
        .shadowMap = readResource<BindlessTexture>(graph, pass, shadowMap, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL),
        .depthMap = readResource<BindlessTexture>(graph, pass, depthMap, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL),
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
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
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
        const auto& colorImage = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(graph,
            data.color));
        auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(colorImage.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const auto& normalImage = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(graph,
            data.normal));
        auto normalAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(normalImage.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const auto& positionImage = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(graph,
            data.positions));
        auto positionAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(positionImage.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const auto& reflectionImage = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(graph,
            data.reflections));
        auto reflectionAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(reflectionImage.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo attachments[] = {
            colorAttachmentInfo,
            normalAttachmentInfo,
            positionAttachmentInfo,
            reflectionAttachmentInfo,
        };
        auto depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(
            backend.bindlessResources->getTexture(
                *getResource<BindlessTexture>(graph, data.depthMap)).view,
                // No clear -- we're using ZPrePass
                VK_ATTACHMENT_LOAD_OP_LOAD);
        auto renderingInfo = vkutil::init::renderingInfo(swapchainSize, attachments, std::size(attachments),
            &depthAttachmentInfo);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [data, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene) -> void
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
            .vertexBufferAddr = backend.getBufferDeviceAddress(scene.vertexBuffer.buffer),
            .perModelDataBufferAddr = backend.getBufferDeviceAddress(scene.perModelBuffer.buffer),
            .shadowData = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.shadowData)),
            .lightList = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.lightList)),
            .lightIndexList = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.lightIndexList)),
            .lightGrid = backend.getBufferDeviceAddress(*getResource<Buffer>(graph, data.lightGrid)),
            .shadowMapIndex = *getResource<BindlessTexture>(graph, data.shadowMap),
            .depthMapIndex = *getResource<BindlessTexture>(graph, data.depthMap),
        };
        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
            &pushConstants);
        vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdBindIndexBuffer(cmd, scene.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, *getResource<Buffer>(graph, data.culledDraws), 0, scene.meshes.size(),
            sizeof(VkDrawIndexedIndirectCommand));
    };

    return ForwardRenderGraphData {
        .color = data.color,
        .normal = data.normal,
        .positions = data.positions,
        .reflections = data.reflections
    };
}
