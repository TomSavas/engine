#include "passes/testPass.h"

#include "renderGraph.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/shader.h"
#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/inits.h"

std::optional<TestRenderer> initTestRenderer(VulkanBackend& backend)
{
    TestRenderer renderer;

    std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(
        backend.device, SHADER_PATH("colored_triangle.vert.glsl"));
    std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(
        backend.device, SHADER_PATH("colored_triangle.frag.glsl"));
    if (!vertexShader || !fragmentShader)
    {
        return std::nullopt;
    }

    VkDescriptorSetLayout descriptors[] = {backend.sceneDescriptorSetLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(descriptors, 1, nullptr, 0);
    VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &renderer.pipeline.pipelineLayout));

    // TODO: convert into optional
    renderer.pipeline.pipeline = PipelineBuilder(backend)
                                     .shaders((*vertexShader)->module, (*fragmentShader)->module)
                                     .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                     .polyMode(VK_POLYGON_MODE_FILL)
                                     .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                                     .disableMultisampling()
                                     .enableAlphaBlending()
                                     .colorAttachmentFormat(backend.backbufferImage.format)
                                     .disableDepthTest()
                                     // .depthFormat(VK_FORMAT_D32_SFLOAT)
                                     // .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                                     .addViewportScissorDynamicStates()
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
        VkExtent2D swapchainSize = {
            static_cast<uint32_t>(backend.viewport.width),
            static_cast<uint32_t>(backend.viewport.height)
        };
        VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(
            backend.backbufferImage.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(
            swapchainSize, &colorAttachmentInfo, 1, nullptr);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene)
    {
        vkCmdDraw(cmd, 3, 1, 0, 0);
    };
}
