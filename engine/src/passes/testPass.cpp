#include "passes/testPass.h"

#include "renderGraph.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/shader.h"
#include "rhi/vulkan/utils/inits.h"

auto initTestRenderer(VulkanBackend& backend) -> TestRenderer
{
    return TestRenderer{
        .pipeline = PipelineBuilder(backend)
            .addDescriptorLayouts({
                backend.sceneDescriptorSetLayout,
            })
            .addShader(SHADER_PATH("colored_triangle.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(SHADER_PATH("colored_triangle.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
            .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .polyMode(VK_POLYGON_MODE_FILL)
            .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .disableMultisampling()
            .enableAlphaBlending()
            .colorAttachmentFormat(backend.DEFAULT_FORMAT)
            .disableDepthTest()
            .addViewportScissorDynamicStates()
            .build()
    };
}

auto testPass(std::optional<TestRenderer>& renderer, VulkanBackend& backend, RenderGraph& graph) -> void
{
    if (!renderer)
    {
        renderer = initTestRenderer(backend);
    }

    auto& pass = createPass(graph);
    pass.pass.debugName = "Test pass";
    pass.pass.pipeline = renderer->pipeline;

    pass.pass.beginRendering = [&backend](const RenderContext& ctx)
    {
        // auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(backend.backbufferImage.view, nullptr,
        //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // const auto renderingInfo = vkutil::init::renderingInfo(ctx.swapchain.size, &colorAttachmentInfo, 1, nullptr);
        // vkCmdBeginRendering(ctx.cmd, &renderingInfo);
    };

    pass.pass.draw = [](const RenderContext& ctx, RenderPass&)
    {
        vkCmdDraw(ctx.cmd, 3, 1, 0, 0);
    };
}
