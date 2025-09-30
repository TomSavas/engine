#include "passes/blur.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"

#include <glm/vec4.hpp>

#include <math.h>

struct DualKawasePushConstants
{
    // xy = resolution reciprocal
    // z = positionOffsetMultiplier
    // w = colorMultiplier
    glm::vec4 params;
    u32 inputTexture;
};

auto initDualKawase(VulkanBackend& backend, RenderGraph& graph) -> BlurRenderer
{
    // TODO: this should be retrieved from render graph
    const auto outputImage = backend.allocateImage(vkutil::init::imageCreateInfo(VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        backend.backbufferImage.extent, 1), VMA_MEMORY_USAGE_GPU_ONLY, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    const auto minDimension = std::min(backend.backbufferImage.extent.width, backend.backbufferImage.extent.height);
    const auto maxLevels = static_cast<u8>(std::log2(static_cast<f32>(minDimension)));
    std::vector<BindlessTexture> intermediateTextures;
    intermediateTextures.reserve(maxLevels - 1);
    for (u8 i = 0; i < maxLevels - 1; i++)
    {
        const auto resolution = VkExtent3D{
            .width = static_cast<u32>(backend.backbufferImage.extent.width / std::pow(2, i + 1)),
            .height = static_cast<u32>(backend.backbufferImage.extent.height / std::pow(2, i + 1)),
            .depth = backend.backbufferImage.extent.depth,
        };
        std::println("intermediate texture: {} x {}", resolution.width, resolution.height);
        const auto intermediateImage = backend.allocateImage(vkutil::init::imageCreateInfo(VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            resolution, 1), VMA_MEMORY_USAGE_GPU_ONLY, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
        intermediateTextures.push_back(
            backend.bindlessResources->addTexture(
                Texture{
                    .image = intermediateImage,
                    .view = intermediateImage.view,
                    .mipCount = 1,
                }
            )
        );
    }

    return BlurRenderer{
        .dualKawaseDownPipeline = PipelineBuilder(backend)
            .addDescriptorLayouts({
                backend.sceneDescriptorSetLayout,
                backend.bindlessResources->bindlessTexDescLayout
            })
            .addPushConstants({
                VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                    .offset = 0,
                    .size = sizeof(glm::vec4)
                },
                VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = sizeof(glm::vec4),
                    .size = sizeof(DualKawasePushConstants)
                }
            })
            .addShader(SHADER_PATH("fullscreen_quad.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(SHADER_PATH("dual_kawase_down.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
            .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .polyMode(VK_POLYGON_MODE_FILL)
            .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .disableMultisampling()
            .enableAlphaBlending()
            .colorAttachmentFormat(backend.backbufferImage.format)
            .addViewportScissorDynamicStates()
            .disableDepthTest()
            .build(),
        .dualKawaseUpPipeline = PipelineBuilder(backend)
            .addDescriptorLayouts({
                backend.sceneDescriptorSetLayout,
                backend.bindlessResources->bindlessTexDescLayout
            })
            .addPushConstants({
                VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                    .offset = 0,
                    .size = sizeof(glm::vec4)
                },
                VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = sizeof(glm::vec4),
                    .size = sizeof(DualKawasePushConstants)
                }
            })
            .addShader(SHADER_PATH("fullscreen_quad.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(SHADER_PATH("dual_kawase_up.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
            .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .polyMode(VK_POLYGON_MODE_FILL)
            .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .disableMultisampling()
            .enableAlphaBlending()
            .colorAttachmentFormat(backend.backbufferImage.format)
            .addViewportScissorDynamicStates()
            .disableDepthTest()
            .build(),
        .output = backend.bindlessResources->addTexture(
            Texture {
                .image = outputImage,
                .view = outputImage.view,
                .mipCount = 1,
            }
        ),
        .intermediateTextures = std::move(intermediateTextures),
    };
}

[[nodiscard]]
auto kawasePass(Pipeline& kawasePipeline, VulkanBackend& backend, RenderGraph& graph, f32 positionOffsetMultiplier,
    f32 colorMultiplier, RenderGraphResource<BindlessTexture> input, BindlessTexture output)
    -> RenderGraphResource<BindlessTexture>
{
    auto& pass = createPass(graph);
    pass.pass.debugName = std::format("Dual Kawase Blur pass");
    pass.pass.pipeline = kawasePipeline;

    input = readResource<BindlessTexture>(graph, pass, input, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    BindlessTexture* hackToNotLooseResource = new BindlessTexture(output);
    auto outputResource = writeResource<BindlessTexture>(graph, pass, importResource(graph, pass,
        hackToNotLooseResource),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    pass.pass.beginRendering = [outputResource, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    {
        const auto& outputTexture = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(graph, outputResource));
        auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(outputTexture.view, nullptr,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto renderingInfo = vkutil::init::renderingInfo(outputTexture.image.extent, &colorAttachmentInfo, 1, nullptr);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [input, outputResource, positionOffsetMultiplier, colorMultiplier, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene)
    {
        ZoneScopedCpuGpuAuto("Dual Kawase Blur pass", backend.currentFrame());
        const auto bindlessInputTexture = *getResource<BindlessTexture>(graph, input);
        const auto& inputTexture = backend.bindlessResources->getTexture(bindlessInputTexture);
        const auto pushConstants = DualKawasePushConstants {
            .params = glm::vec4(
                1.f / static_cast<float>(inputTexture.image.extent.width),
                1.f / static_cast<float>(inputTexture.image.extent.height),
                positionOffsetMultiplier,
                colorMultiplier
            ),
            .inputTexture = bindlessInputTexture
        };
        const auto depth = glm::vec4(0.f);
        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec4),
            &depth);
        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec4), sizeof(pushConstants),
            &pushConstants);

        const auto& outputTexture = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(graph, outputResource));
        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<f32>(outputTexture.image.extent.width),
            .height = static_cast<f32>(outputTexture.image.extent.height),
            .maxDepth = 1.f
        };
        VkRect2D scissor = {
            .offset = VkOffset2D{0, 0},
            .extent = VkExtent2D{outputTexture.image.extent.width, outputTexture.image.extent.height}
        };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    };

    return outputResource;
}

[[nodiscard]]
auto dualKawaseBlur(std::optional<BlurRenderer>& blur, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<BindlessTexture> input, u8 downsampleCount, f32 positionOffsetMultiplier, f32 colorMultiplier)
    -> RenderGraphResource<BindlessTexture>
{
    if (!blur)
    {
        blur = initDualKawase(backend, graph);
    }

    for (i8 i = 0; i < downsampleCount; i++)
    {
        const auto outputTexture = blur->intermediateTextures[i];
        input = kawasePass(blur->dualKawaseDownPipeline, backend, graph, positionOffsetMultiplier, colorMultiplier,
            input, outputTexture);
    }

    for (i8 i = downsampleCount - 2; i > 0; i--)
    {
        const auto outputTexture = blur->intermediateTextures[i];
        input = kawasePass(blur->dualKawaseUpPipeline, backend, graph, positionOffsetMultiplier, colorMultiplier,
            input, outputTexture);
    }

    const auto output = kawasePass(blur->dualKawaseUpPipeline, backend, graph,
        positionOffsetMultiplier, colorMultiplier, input, blur->output);

    return output;
}