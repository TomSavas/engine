#include "passes/sdf.h"

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

struct SdfPushConstants
{
    u32 colorIndex;
};

auto initSdf(VulkanBackend& backend) -> SdfRenderer
{
    return SdfRenderer{
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
                // VkPushConstantRange {
                //     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                //     .offset = 0,
                //     .size = sizeof(SdfPushConstants)
                // }
            })
            .addShader(SHADER_PATH("fullscreen_quad.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(SHADER_PATH("sdf_scene.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
            .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .polyMode(VK_POLYGON_MODE_FILL)
            .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .disableMultisampling()
            .enableAlphaBlending()
            .colorAttachmentFormat(backend.DEFAULT_FORMAT)
            .addViewportScissorDynamicStates()
            .depthFormat(VK_FORMAT_D32_SFLOAT) // TEMP: this should be taken from bindless
            .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
            .build()
    };
}

auto sdfGeometryPass(std::optional<SdfRenderer>& sdfRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> color, RenderGraphResource<BindlessTexture> depth)
    // -> SdfRenderGraphData
    -> RenderGraphResource<BindlessTexture>
{
    if (!sdfRenderer)
    {
        sdfRenderer = initSdf(backend);
    }

    auto& pass = createPass(graph);
    pass.pass.debugName = "SDF pass";
    pass.pass.pipeline = sdfRenderer->pipeline;

    auto output = writeResource<BindlessTexture>(graph, pass, color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    auto d = writeResource<BindlessTexture>(graph, pass, depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
 
    pass.pass.beginRendering = [output, d, &backend](const RenderContext& ctx)
    {
        const auto& colorImage = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(ctx.graph,
            output));
        auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(colorImage.image.view, nullptr,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(
            backend.bindlessResources->getTexture(
                *getResource<BindlessTexture>(ctx.graph, d)).image.view,
                // No clear -- we're using ZPrePass
                VK_ATTACHMENT_LOAD_OP_LOAD,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        auto renderingInfo = vkutil::init::renderingInfo(ctx.swapchain.size, &colorAttachmentInfo, 1,
            &depthAttachmentInfo);
        vkCmdBeginRendering(ctx.cmd, &renderingInfo);
    };

    pass.pass.draw = [output, &backend](const RenderContext& ctx, RenderPass& pass) -> void
    {
        ZoneScopedCpuGpuAuto("SDF scene pass", backend.currentFrame());
        
        constexpr glm::vec4 depth = glm::vec4(0.f);
        vkCmdPushConstants(ctx.cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec4),
            &depth);
        vkCmdBindDescriptorSets(ctx.cmd, pass.pipeline->pipelineBindPoint, pass.pipeline->pipelineLayout, 1, 1,
            &backend.bindlessResources->bindlessTexDesc, 0, nullptr);
        vkCmdDraw(ctx.cmd, 3, 1, 0, 0);
    };

    return output;
}
