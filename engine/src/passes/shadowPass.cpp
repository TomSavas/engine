#include "passes/passes.h"
#include "scene.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"

#include "imgui.h"

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

struct PushConstants
{
    VkDeviceAddress vertexBufferAddr;
    VkDeviceAddress cascadeDataAddr;
    int cascade;
};

std::optional<ShadowRenderer> initCsm(RHIBackend& backend)
{
    ShadowRenderer renderer;

    std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("simple_shadowpass.vert.glsl"));
    std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("empty.frag.glsl"));
    if (!vertexShader || !fragmentShader)
    {
        return std::optional();
    }

    VkPushConstantRange meshPushConstantRange = vkutil::init::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstants));
    // VkDescriptorSetLayout descriptors[] = {backend.sceneDescriptorSetLayout, backend.bindlessTexDescLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(nullptr, 0, &meshPushConstantRange, 1);
    VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pass.pipeline->pipelineLayout));

    // TODO: convert into optional
    renderer.pipeline = PipelineBuilder()
        .shaders((*vertexShader)->module, (*fragmentShader)->module)
        .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .polyMode(VK_POLYGON_MODE_FILL)
        .cullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE) // Front face!
        .disableMultisampling()
        .disableBlending()
        .depthFormat(backend.depthImage.format)
        .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
        .setDepthClamp(true)
        .build(backend.device, pass.pipeline->pipelineLayout);

    // Persistent data
    auto info = vkutil::init::bufferCreateInfo(sizeof(ShadowPassData),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    renderer.shadowMapData = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    constexpr int shadowMapSize = 1024;
    AllocatedImage shadowMapImage = backend.allocateImage(
        vkutil::init::imageCreateInfo(
            VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            { cascadeCount * shadowMapSize, shadowMapSize, 1 }),
        VMA_MEMORY_USAGE_GPU_ONLY,
        0, // NOTE: this might cause issues
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    Texture t = backend.textures.loadRaw(normalImg.image.data(), normalImg.image.size(),
        normalImg.width, normalImg.height, true, false, normalImg.name);
    renderer.shadowMap = backend.bindless.addTexture(t);
}

ShadowPassRenderGraphData csmPass(std::optional<ShadowRenderer>& shadowRenderer, RHIBackend& backend, RenderGraph& graph, int cascadeCount)
{
    if (shadowRenderer)
    {
        shadowRenderer = initCsm();
    }

    RenderPass& pass = createPass(graph);
    pass.debugName = "CSM pass";
    pass.pipeline = shadowRenderer.pipeline;
    pass.pipeline->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // RenderGraph resources
    ShadowPassRenderGraphData data;
    data.shadowMap = importResource<BindlessTexture>(graph, pass, shadowRenderer->shadowMap);
    // NOTE: potentially we can change this to writeAttachmentResource to automatically generate
    // rendering info.
    data.shadowMap = writeResource<BindlessTexture>(graph, pass, data.shadowMap);
    data.data = importResource<Buffer>(graph, pass, shadowRenderer->shadowMapData);
    data.data = writeResource<Buffer>(graph, pass, data.data);

    pass.beginRendering = [data](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass) {
        const BindlessTexture shadowMap = graph.getResource(data.shadowMap);
        const VkExtent2d size = {shadowMap.width, shadowMap.height};
        
        VkRenderingAttachmentInfo depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(shadowMap.view);
        VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(size, nullptr, 0, &depthAttachmentInfo);
        VkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.draw = [data, cascadeCount](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene) {
        data.cascadeCount = cascadeCount;
        csmLightViewProjMats(data.lightViewProjMatrices, data.cascadeDistances, cascadeCount, scene.mainCamera.view(), scene.mainCamera.proj(), scene.lightDir,
            scene.mainCamera.nearClippingPlaneDist, scene.mainCamera.farClippingPlaneDist, cascadeSplitLambda);
        for (int i = 0; i < cascadeCount; ++i)
        {
            data.invLightViewProjMatrices[i] = glm::inverse(data.lightViewProjMatrices[i]);
        }
        backend.copyBufferWithStaging((void*)&data, sizeof(ShadowPassData), dataBuffer);

        const VkBuffer dataBuffer = getResource(graph, data.data);
        PushConstants pushConstants 
        {
            .vertexBufferAddr = backend.getBufferDeviceAddress(scene.vertexBuffer),
            .cascadeDataAddr = backend.getBufferDeviceAddress(dataBuffer),
        };

        const BindlessTexture shadowMap = graph.getResource(data.shadowMap);
        const VkExtent2d size = {shadowMap.width, shadowMap.height};
        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = size.width,
            .height = size.height,
            .maxDepth = 1.f
        };
        VkRect2D scissor = {
            .offset = VkOffset2D{0, 0},
            .extent = size
        };

    	vkCmdBindIndexBuffer(cmd, internalData->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    	for (int i = 0; i < cascadeCount; ++i)
    	{
    	    pushConstants.cascade = i;
            vkCmdPushConstants(cmd, p.pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPassPushConstants), &pushConstants);

            viewport.x = i * shadowMapSize;
        	scissor.offset.x = viewport.x;
        	vkCmdSetViewport(cmd, 0, 1, &viewport);
        	vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdDrawIndexedIndirect(cmd, internalData->indirectCommands.buffer, 0, scene.meshes.size(), sizeof(VkDrawIndexedIndirectCommand));
    	}
    }

    return data;
}
