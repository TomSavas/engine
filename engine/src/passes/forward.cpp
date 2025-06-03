#include "passes/passes.h"
#include "scene.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"

#include "imgui.h"

#include <vulkan/vulkan_core.h>
#include <glm/gtx/transform.hpp>

#include "GLFW/glfw3.h"

struct PushConstants
{
    VkDeviceAddress vertexBufferAddr;
    VkDeviceAddress perModelDataBufferAddr;
    VkDeviceAddress shadowData;
    int shadowMapIndex;
};

std::optional<ForwardOpaqueRenderer> initForwardOpaque(RHIBackend& backend)
{
    ForwardOpaqueRenderer renderer;
    
    std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("mesh.vert.glsl"));
    std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("mesh.frag.glsl"));
    if (!vertexShader || !fragmentShader)
    {
        return std::optional<ForwardOpaqueRenderer>();
    }

    VkPushConstantRange meshPushConstantRange = vkutil::init::pushConstantRange(VK_SHADER_STAGE_ALL, sizeof(PushConstants));
    VkDescriptorSetLayout descriptors[] = {backend.sceneDescriptorSetLayout, backend.bindlessTexDescLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(descriptors, 2, &meshPushConstantRange, 1);
    VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, renderer.pipeline->pipelineLayout));

    // TODO: convert into optional
    renderer.pipeline = PipelineBuilder()
        .shaders((*vertexShader)->module, (*fragmentShader)->module)
        .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .polyMode(VK_POLYGON_MODE_FILL)
        .cullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .disableMultisampling()
        .enableAlphaBlending()
        .colorAttachmentFormat(backend.backbufferImage.format)
        .depthFormat(backend.depthImage.format)
        .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
        .build(backend.device, pass.pipeline->pipelineLayout);
    
    return renderer;
}

void opaqueForwardPass(std::optional<ForwardOpaqueRenderer>& forwardOpaqueRenderer, RHIBackend& backend, RenderGraph& graph,
    RenderGraphResource<Buffer> culledDraws, RenderGraphResource<Buffer> shadowData,
    RenderGraphResource<BindlessTexture> shadowMap)
{
    if (forwardOpaqueRenderer)
    {
        forwardOpaqueRenderer = initForwardOpaque();
    }

    RenderPass& pass = createPass(graph);
    pass.debugName = "Forward Opaque pass";
    pass.pipeline = forwardOpaque.pipeline;
    pass.pipeline->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // RenderGraph resources
    struct ForwardOpaqueRenderGraphData
    {
        RenderGraphResource<Buffer> culledDraws;
        RenderGraphResource<Buffer> shadowData;
        RenderGraphResource<BindlessTexture> shadowMap;
    } data;
    data.culledDraws = readResource<Buffer>(graph, pass, culledDraws);
    data.shadowData = readResource<Buffer>(graph, pass, shadowData);
    data.shadowMap = readResource<BindlessTexture>(graph, pass, shadowMap);

    pass.beginRendering = [data](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass)
    {
        // TODO: attach depth from Z prepass
        VkExtent2D swapchainSize { static_cast<uint32_t>(backend.viewport.width), static_cast<uint32_t>(backend.viewport.height) };
        VkClearValue colorClear = {
           .color = {
               .uint32 = {0, 0, 0, 0}
           }
        };
        VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(backend.backbufferImage.view, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(backend.depthImage.view);
        VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(swapchainSize, &colorAttachmentInfo, 1, &depthAttachmentInfo);
        VkCmdBeginRendering(cmd, &renderingInfo);
    };
    
    pass.draw = [data](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene) {
    {
        PushConstants pushConstants 
        {
            .vertexBufferAddr = backend.getBufferDeviceAddress(scene.vertexBuffer)
            // .perModelDataBufferAddr = vkGetBufferDeviceAddress(backend.device, &perModelDataAddressInfo),
            .shadowData = backend.getBufferDeviceAddress(getResource(graph, data.shadowData)),
            .shadowMapIndex = getResource(graph, data.shadowMap)
        };
        vkCmdPushConstants(cmd, p.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pushConstants);
    	vkCmdBindIndexBuffer(cmd, basePassData->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, culledDraws.buffer, 0, scene.meshes.size(), sizeof(VkDrawIndexedIndirectCommand));
    }
}
