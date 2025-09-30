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

    pass.pass.beginRendering = [&backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    {
        VkExtent2D swapchainSize = {
            static_cast<u32>(backend.viewport.width),
            static_cast<u32>(backend.viewport.height)
        };
        VkClearValue colorClear = {
            .color = {.uint32 = {0, 0, 0, 0}}
        };
        auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(backend.backbufferImage.view, &colorClear,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const auto renderingInfo = vkutil::init::renderingInfo(swapchainSize, &colorAttachmentInfo, 1, nullptr);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [input, blurredInput, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene)
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
            .blurredInput = *getResource<BindlessTexture>(graph, blurredInput),
            .input = *getResource<BindlessTexture>(graph, input),
        };
        const auto depth = glm::vec4(0.f);
        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec4),
            &depth);
        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec4), sizeof(pushConstants),
            &pushConstants);

        vkCmdBindDescriptorSets(cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    };

    // TEMP: replace with actual resource
    return 0;
}
