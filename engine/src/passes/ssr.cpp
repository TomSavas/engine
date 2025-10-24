#include "passes/screenSpace.h"

#include "debugUI.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"

#include <glm/vec4.hpp>

#include <math.h>

struct SsrPushConstants
{
    u32 color;
    u32 normal;
    u32 positions;
    u32 reflections;
    u32 blurredReflections;
    u32 mode;
    f32 reflectionIntensity;
    f32 blurIntensity;
};

auto initScreenSpace(VulkanBackend& backend) -> ScreenSpaceRenderer
{
    return ScreenSpaceRenderer{
        .ssrPipeline = PipelineBuilder(backend)
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
                    .size = sizeof(SsrPushConstants)
                }
            })
            .addShader(SHADER_PATH("fullscreen_quad.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(SHADER_PATH("post_ssr.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
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
            backend.allocateTexture(
                "SSR output",
                vkutil::init::defaultColorAttachmentTextureCreateInfo(backend.backbufferImage.extent),
                vkutil::init::defaultTextureAllocationCreateInfo(),
                MipOptions::one(),
                VK_IMAGE_ASPECT_COLOR_BIT
            )
        ),
    };
}

auto ssrPass(std::optional<ScreenSpaceRenderer>& ssRenderer, std::optional<BlurRenderer>& blur, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> colorOutput, RenderGraphResource<BindlessTexture> normal,
    RenderGraphResource<BindlessTexture> positions, RenderGraphResource<BindlessTexture> reflectionUvs)
    -> RenderGraphResource<BindlessTexture>
{
    // Blur reflections
    // TODO: make this configurable from imgui. But that requires recompiling render graph every frame
    auto blurredReflectionUvs = dualKawaseBlur("ssr reflection blur", blur, backend, graph, reflectionUvs, 1);

    if (!ssRenderer)
    {
        ssRenderer = initScreenSpace(backend);
    }
    auto& pass = createPass(graph);
    pass.pass.debugName = "SSR pass";
    pass.pass.pipeline = ssRenderer->ssrPipeline;

    colorOutput = readResource<BindlessTexture>(graph, pass, colorOutput, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    normal = readResource<BindlessTexture>(graph, pass, normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    positions = readResource<BindlessTexture>(graph, pass, positions, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    reflectionUvs = readResource<BindlessTexture>(graph, pass, reflectionUvs, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    blurredReflectionUvs = readResource<BindlessTexture>(graph, pass, blurredReflectionUvs, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto output = writeResource<BindlessTexture>(graph, pass, importResource(graph, pass, &ssRenderer->output),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    pass.pass.beginRendering = [output, &backend](const RenderContext& ctx)
    {
        VkClearValue colorClear = {
            .color = {.uint32 = {0, 0, 0, 0}}
        };
        const auto& outputTexture = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(ctx.graph, output));
        auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(outputTexture.image.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto renderingInfo = vkutil::init::renderingInfo(ctx.swapchain.size, &colorAttachmentInfo, 1, nullptr);
        vkCmdBeginRendering(ctx.cmd, &renderingInfo);
    };

    pass.pass.draw = [colorOutput, normal, positions, reflectionUvs, blurredReflectionUvs, &backend](const RenderContext& ctx, RenderPass& pass)
    {
        ZoneScopedCpuGpuAuto("SSR pass", backend.currentFrame());

        static enum Mode
        {
            REFLECTED_UVS = 0,
            MASKED_REFLECTED_UVS,
            COLOR,
            REFLECTIONS,
            BLEND,
            ERROR
        } mode = BLEND;
        static float reflectionIntensity = 2.f;
        static float blurIntensity = 0.75f;
        addDebugUI(debugUI, GRAPHICS_PASSES, [&]()
        {
            if (ImGui::TreeNode("Screen Space Reflections"))
            {
                ImGui::SliderFloat("Reflection intensity", &reflectionIntensity, 0.f, 50.f, "%.3f");
                ImGui::SliderFloat("Reflection blur intensity", &blurIntensity, 0.f, 1.f, "%.5f");

                if (ImGui::RadioButton("Reflected UVs", mode == REFLECTED_UVS)) { mode = REFLECTED_UVS; }
                if (ImGui::RadioButton("Masked reflected UVs", mode == MASKED_REFLECTED_UVS)) { mode = MASKED_REFLECTED_UVS; }
                if (ImGui::RadioButton("Color only", mode == COLOR)) { mode = COLOR; }
                if (ImGui::RadioButton("Reflections only", mode == REFLECTIONS)) { mode = REFLECTIONS; }
                if (ImGui::RadioButton("Blended", mode == BLEND)) { mode = BLEND; }
                if (ImGui::RadioButton("Error", mode == ERROR)) { mode = ERROR; }

                ImGui::TreePop();
            }
        });

        constexpr glm::vec4 depth = glm::vec4(0.f);
        const SsrPushConstants pushConstants = {
            .color = *getResource<BindlessTexture>(ctx.graph, colorOutput),
            .normal = *getResource<BindlessTexture>(ctx.graph, normal),
            .positions = *getResource<BindlessTexture>(ctx.graph, positions),
            .reflections = *getResource<BindlessTexture>(ctx.graph, reflectionUvs),
            .blurredReflections = *getResource<BindlessTexture>(ctx.graph, blurredReflectionUvs),
            .mode = static_cast<u32>(mode),
            .reflectionIntensity = reflectionIntensity,
            .blurIntensity = blurIntensity
        };
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
