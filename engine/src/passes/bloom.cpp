#include "passes/bloom.h"

#include "debugUI.h"
#include "glm/vec4.hpp"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"

struct PushConstants
{
    glm::vec4 strength;
    u32 blurredInput;
    u32 input;
};

auto initBloom(VulkanBackend& backend) -> BloomRenderer
{
    const auto outputImage = backend.allocateImage(vkutil::init::imageCreateInfo(VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        backend.backbufferImage.extent, 1), VMA_MEMORY_USAGE_GPU_ONLY, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    return BloomRenderer{
        .pipeline = PipelineBuilder(backend)
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
                    .size = sizeof(PushConstants)
                }
            })
            .addShader(SHADER_PATH("fullscreen_quad.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(SHADER_PATH("bloom_blend.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
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
    };
}

[[nodiscard]]
auto bloomPass(std::optional<BloomRenderer>& bloom, std::optional<BlurRenderer>& blur, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> input)
    -> RenderGraphResource<BindlessTexture>
{
    // Blur reflections
    // TODO: make this configurable from imgui. But that requires recompiling render graph every frame
    auto blurredInput = dualKawaseBlur(blur, backend, graph, input, 4);

    if (!bloom)
    {
        bloom = initBloom(backend);
    }

    auto& pass = createPass(graph);
    pass.pass.debugName = std::format("Bloom pass");
    pass.pass.pipeline = bloom->pipeline;

    input = readResource<BindlessTexture>(graph, pass, input, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    blurredInput = readResource<BindlessTexture>(graph, pass, blurredInput, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto output = writeResource<BindlessTexture>(graph, pass, importResource(graph, pass, &bloom->output),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    pass.pass.beginRendering = [&backend, output](const RenderContext& ctx)
    {
        VkClearValue colorClear = {
            .color = {.uint32 = {0, 0, 0, 0}}
        };
        const auto& outputTexture = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(ctx.graph, output));
        auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(outputTexture.view, nullptr,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(backend.backbufferImage.view, &colorClear,
        //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const auto renderingInfo = vkutil::init::renderingInfo(ctx.swapchain.size, &colorAttachmentInfo, 1, nullptr);
        vkCmdBeginRendering(ctx.cmd, &renderingInfo);
    };

    pass.pass.draw = [input, blurredInput, &backend](const RenderContext& ctx, RenderPass& pass)
    {
        ZoneScopedCpuGpuAuto("Bloom pass", backend.currentFrame());

        static float bloomIntensity = 0.04f;
        addDebugUI(debugUI, GRAPHICS_PASSES, [&]()
        {
            if (ImGui::TreeNode("Bloom"))
            {
                ImGui::SliderFloat("Intensity", &bloomIntensity, 0.f, 1.f, "%.5f");
                ImGui::TreePop();
            }
        });

        const auto pushConstants = PushConstants {
            .strength = glm::vec4(bloomIntensity),
            .blurredInput = *getResource<BindlessTexture>(ctx.graph, blurredInput),
            .input = *getResource<BindlessTexture>(ctx.graph, input),
        };
        const auto depth = glm::vec4(0.f);
        vkCmdPushConstants(ctx.cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec4),
            &depth);
        vkCmdPushConstants(ctx.cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec4), sizeof(pushConstants),
            &pushConstants);

        vkCmdBindDescriptorSets(ctx.cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdDraw(ctx.cmd, 3, 1, 0, 0);
    };

    return output;
}
