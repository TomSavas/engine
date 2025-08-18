#include "passes/atmosphere.h"

#include "glm/ext/scalar_constants.hpp"
#include "imgui.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"
#include "scene.h"
#include "tracy/Tracy.hpp"

struct PushConstants
{
    glm::vec4 depth;
    glm::vec4 sunDir;
};

std::optional<AtmosphereRenderer> initAtmosphere(VulkanBackend& backend)
{
    AtmosphereRenderer renderer;

    VkDescriptorSetLayout descriptors[] = {backend.sceneDescriptorSetLayout};
    VkPushConstantRange pushConstants = vkutil::init::pushConstantRange(VK_SHADER_STAGE_ALL, sizeof(PushConstants));
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(descriptors, 1, &pushConstants, 1);
    VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &renderer.pipeline.pipelineLayout));

    renderer.pipeline.pipeline = PipelineBuilder(backend)
                                     .addShader(SHADER_PATH("fullscreen_quad.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
                                     .addShader(SHADER_PATH("atmosphere.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
                                     .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                     .polyMode(VK_POLYGON_MODE_FILL)
                                     .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                                     .disableMultisampling()
                                     .enableAlphaBlending()
                                     .colorAttachmentFormat(backend.backbufferImage.format)
                                     .depthFormat(VK_FORMAT_D32_SFLOAT)
                                     .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                                     .addViewportScissorDynamicStates()
                                     .build(backend.device, renderer.pipeline.pipelineLayout);
    renderer.pipeline.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    return renderer;
}

void atmospherePass(std::optional<AtmosphereRenderer>& atmosphere, VulkanBackend& backend, RenderGraph& graph, RenderGraphResource<BindlessTexture> depthMap)
{
    if (!atmosphere)
    {
        atmosphere = initAtmosphere(backend);
    }

    RenderGraph::Node& pass = createPass(graph);
    pass.pass.debugName = "Atmosphere pass";
    pass.pass.pipeline = atmosphere->pipeline;

    depthMap = readResource<BindlessTexture>(graph, pass, depthMap, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL),

    pass.pass.beginRendering = [depthMap, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    {
        VkExtent2D swapchainSize = {
            static_cast<uint32_t>(backend.viewport.width),
            static_cast<uint32_t>(backend.viewport.height)
        };
        VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(
            backend.backbufferImage.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(
            backend.bindlessResources->getTexture(
                *getResource<BindlessTexture>(graph, depthMap)).view,
                VK_ATTACHMENT_LOAD_OP_LOAD);
        VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(
            swapchainSize, &colorAttachmentInfo, 1, &depthAttachmentInfo);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [&backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene)
    {
        static float time = 0.f;
        static bool moveSun = false;
        static float speed = 0.0008f;
        static float sign = 1.f;
        //time += 0.005f;
        //time += std::max(pow(cos(time), 4) * 0.25, 0.0001);

        if (ImGui::Begin("Atmosphere"))
        {
            ImGui::SliderFloat("Time", &time, 0, 2 * glm::pi<float>());
            ImGui::Checkbox("Sun movement", &moveSun);
            ImGui::SliderFloat("Speed", &speed, 0.0001f, 0.001f);
        }
        ImGui::End();

        if (moveSun)
        {
            if (time > glm::pi<float>() / 2.f + 0.2f || time < 0.f)
            {
                sign *= -1.f;
            }
            time += speed * sign;
        }
        scene.lightDir = glm::normalize(glm::vec3(-sin(time), -cos(time), 0.f));

        ZoneScopedCpuGpuAuto("Atmosphere pass", backend.currentFrame());
        PushConstants pushConstants = {
            .depth = glm::vec4(1.f),
            .sunDir = glm::vec4(-scene.lightDir.x, -scene.lightDir.y, scene.lightDir.z, 0.f),
        };
        vkCmdPushConstants(
            cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pushConstants);

        vkCmdDraw(cmd, 3, 1, 0, 0);
    };
}
