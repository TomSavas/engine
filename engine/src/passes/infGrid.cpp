#include "passes/passes.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/inits.h"

#include <vulkan/vulkan_core.h>

std::optional<RenderPass> infGrid(VulkanBackend& backend) {
    RenderPass pass;
    pass.debugName = "infinite grid";
    pass.pipeline = std::optional<RenderPass::Pipeline>(RenderPass::Pipeline{});
    pass.pipeline->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("inf_grid.vert.glsl"));
    std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("inf_grid.frag.glsl"));
    if (!vertexShader || !fragmentShader)
    {
        return std::optional<RenderPass>();
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(&backend.sceneDescriptorSetLayout, 1);
    VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pass.pipeline->pipelineLayout));

    pass.pipeline->pipeline = PipelineBuilder()
        .shaders((*vertexShader)->module, (*fragmentShader)->module)
        .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .polyMode(VK_POLYGON_MODE_FILL)
        .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
        .disableMultisampling()
        .enableAlphaBlending()
        .disableDepthTest()
        .colorAttachmentFormat(backend.backbufferImage.format)
        .depthFormat(backend.depthImage.format)
        .build(backend.device, pass.pipeline->pipelineLayout);

    // Here we should set up the resources in the rendergraph

    pass.draw = [](VkCommandBuffer cmd, RenderPass& p){
        vkCmdDraw(cmd, 6, 1, 0, 0);
    };

    return pass;
}
