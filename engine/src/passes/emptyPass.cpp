#include "passes/passes.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/inits.h"

#include <vulkan/vulkan_core.h>

RenderPass emptyPass(VulkanBackend& backend) {
    RenderPass pass;
    pass.debugName = "empty pass";
    pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("empty.vert.glsl"));
    std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("empty.frag.glsl"));

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(&backend.sceneDescriptorSetLayout, 1);
    VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pass.pipelineLayout));

    pass.pipeline = PipelineBuilder()
        .shaders((*vertexShader)->module, (*fragmentShader)->module)
        .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .polyMode(VK_POLYGON_MODE_FILL)
        .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
        .disableMultisampling()
        .enableAlphaBlending()
        .disableDepthTest()
        .colorAttachmentFormat(backend.backbufferImage.format)
        .depthFormat(VK_FORMAT_UNDEFINED)
        .build(backend.device, pass.pipelineLayout);

    pass.draw = [](VkCommandBuffer cmd, RenderPass& p){};

    return pass;
}
