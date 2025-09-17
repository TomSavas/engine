#include "debugUI.h"
#include "passes/screenSpace.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/inits.h"

struct SsrPushConstants
{
    u32 color;
    u32 normal;
    u32 reflections;
    u32 mode;
};

auto initScreenSpace(VulkanBackend& backend) -> std::optional<ScreenSpaceRenderer>
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
    };
}

auto ssrPass(std::optional<ScreenSpaceRenderer>& ssRenderer, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<BindlessTexture> colorOutput, RenderGraphResource<BindlessTexture> normal,
    RenderGraphResource<BindlessTexture> reflectionUvs)
    -> RenderGraphResource<BindlessTexture>
{
    if (!ssRenderer)
    {
        ssRenderer = initScreenSpace(backend);
    }
    auto& pass = createPass(graph);
    pass.pass.debugName = "SSR pass";
    pass.pass.pipeline = ssRenderer->ssrPipeline;

    colorOutput = readResource<BindlessTexture>(graph, pass, colorOutput, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    normal = readResource<BindlessTexture>(graph, pass, normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    reflectionUvs = readResource<BindlessTexture>(graph, pass, reflectionUvs, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    pass.pass.beginRendering = [&backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    {
        const VkExtent2D swapchainSize = {
            static_cast<u32>(backend.viewport.width),
            static_cast<u32>(backend.viewport.height)
        };
        VkClearValue colorClear = {
            .color = {.uint32 = {0, 0, 0, 0}}
        };
        auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(backend.backbufferImage.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto renderingInfo = vkutil::init::renderingInfo(swapchainSize, &colorAttachmentInfo, 1, nullptr);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [colorOutput, normal, reflectionUvs, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene)
    {
        ZoneScopedCpuGpuAuto("SSR pass", backend.currentFrame());

        static enum Mode
        {
            REFLECTED_UVS = 0,
            COLOR,
            REFLECTIONS,
            BLEND,
            ERROR
        } mode = BLEND;
        addDebugUI(debugUI, GRAPHICS_PASSES, [&]()
        {
            if (ImGui::TreeNode("Screen Space Reflections"))
            {
                if (ImGui::RadioButton("Refected UVs", mode == REFLECTED_UVS)) { mode = REFLECTED_UVS; }
                if (ImGui::RadioButton("Color only", mode == COLOR)) { mode = COLOR; }
                if (ImGui::RadioButton("Reflections only", mode == REFLECTIONS)) { mode = REFLECTIONS; }
                if (ImGui::RadioButton("Blended", mode == BLEND)) { mode = BLEND; }
                if (ImGui::RadioButton("Error", mode == ERROR)) { mode = ERROR; }

                ImGui::TreePop();
            }
        });

        constexpr glm::vec4 depth = glm::vec4(0.f);
        const SsrPushConstants pushConstants = {
            .color = *getResource<BindlessTexture>(graph, colorOutput),
            .normal = *getResource<BindlessTexture>(graph, normal),
            .reflections = *getResource<BindlessTexture>(graph, reflectionUvs),
            .mode = static_cast<u32>(mode),
        };
        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec4),
            &depth);
        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec4), sizeof(pushConstants),
            &pushConstants);
        vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    };

    return 0;
}
