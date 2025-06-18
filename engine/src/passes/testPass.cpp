#include "passes/testPass.h"

#include "rhi/vulkan/shader.h"

#include "render_graph.h"
#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipeline_builder.h"

std::optional<TestRenderer> initTestRenderer(VulkanBackend& backend)
{
    TestRenderer renderer;

    std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("colored_triangle.vert.glsl"));
    std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("colored_triangle.frag.glsl"));
    if (!vertexShader || !fragmentShader)
    {
        return std::optional<TestRenderer>();
    }

    VkDescriptorSetLayout descriptors[] = {backend.sceneDescriptorSetLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(descriptors, 1, nullptr, 0);
    VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &renderer.pipeline.pipelineLayout));

    // TODO: convert into optional
    renderer.pipeline.pipeline = PipelineBuilder()
        .shaders((*vertexShader)->module, (*fragmentShader)->module)
        .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .polyMode(VK_POLYGON_MODE_FILL)
        .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .disableMultisampling()
        .enableAlphaBlending()
        .colorAttachmentFormat(backend.backbufferImage.format)
        .depthFormat(backend.depthImage.format)
        .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
        .build(backend.device, renderer.pipeline.pipelineLayout);
    renderer.pipeline.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    return renderer;
}

void testPass(std::optional<TestRenderer> renderer, VulkanBackend& backend, RenderGraph& graph)
{
    if (!renderer)
    {
        renderer = initTestRenderer(backend);
    }

    RenderGraph::Node& pass = createPass(graph);
    pass.pass.debugName = "Test pass";
    pass.pass.pipeline = renderer->pipeline;

    pass.pass.beginRendering = [&backend](VkCommandBuffer cmd, CompiledRenderGraph&)
    {
        VkExtent2D swapchainSize { static_cast<uint32_t>(backend.viewport.width), static_cast<uint32_t>(backend.viewport.height) };
        VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(backend.backbufferImage.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(backend.depthImage.view);
        VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(swapchainSize, &colorAttachmentInfo, 1, &depthAttachmentInfo);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene)
    {
        vkCmdDraw(cmd, 3, 1, 0, 0);
    };
}
