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

struct InternalShadowPassData 
{
    // For the time being duplicate between this and basepass
    AllocatedBuffer vertexDataBuffer;
    AllocatedBuffer indexBuffer;    
    // This should be kept in the scene most likely
    AllocatedBuffer indirectCommands;
    bool initialized = false;
};

struct ShadowPassPushConstants
{
    // glm::mat4 lightViewProj;
    VkDeviceAddress vertexBufferAddr;
    VkDeviceAddress dataBufferAddr;
    int cascade;
};

std::array<glm::vec3, 8> frustumCornersInWorldSpace(glm::mat4 invViewProj)
{
    std::array<glm::vec3, 8> frustumCorners = {
		glm::vec3(-1.0f,  1.0f, 0.0f),
		glm::vec3( 1.0f,  1.0f, 0.0f),
		glm::vec3( 1.0f, -1.0f, 0.0f),
		glm::vec3(-1.0f, -1.0f, 0.0f),
		glm::vec3(-1.0f,  1.0f,  1.0f),
		glm::vec3( 1.0f,  1.0f,  1.0f),
		glm::vec3( 1.0f, -1.0f,  1.0f),
		glm::vec3(-1.0f, -1.0f,  1.0f),
	};

	for (uint32_t i = 0; i < 8; i++) {
		glm::vec4 invCorner = invViewProj * glm::vec4(frustumCorners[i], 1.0f);
		frustumCorners[i] = invCorner / invCorner.w;
	}

	return frustumCorners;
}

void csmLightViewProjMats(glm::mat4* viewProjMats, float* cascadeDistances, int cascadeCount, glm::mat4 view, glm::mat4 proj, glm::vec3 lightDir, float nearClip, float farClip, float cascadeSplitLambda)
{
	float cascadeSplits[cascadeCount];

	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < cascadeCount; i++) 
	{
		float p = (i + 1) / static_cast<float>(cascadeCount);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		// cascadeDistances[i] = (d - nearClip) / clipRange;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	float lastSplitDist = 0.f;
	glm::mat4 invViewProj = glm::inverse(proj * view);
	for (int i = 0; i < cascadeCount; i++)
	{
        std::array<glm::vec3, 8> frustumCorners = frustumCornersInWorldSpace(invViewProj);
	    
	    float splitDist = cascadeSplits[i];
    	for (uint32_t j = 0; j < 4; j++) {
    		glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
    		frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
    		frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
    	}
    	lastSplitDist = cascadeSplits[i];

    	glm::vec3 frustumCenter = glm::vec3(0.0f);
    	for (uint32_t j = 0; j < 8; j++) {
    		frustumCenter += frustumCorners[j];
    	}
    	frustumCenter /= 8.0f;

    	float radius = 0.0f;
    	for (uint32_t j = 0; j < 8; j++) {
    		float distance = glm::length(frustumCorners[j] - frustumCenter);
    		radius = glm::max(radius, distance);
    	}
    	radius = std::ceil(radius * 16.0f) / 16.0f;

    	glm::vec3 maxExtents = glm::vec3(radius);
    	glm::vec3 minExtents = -maxExtents;

    	glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
    	glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, -(maxExtents.z - minExtents.z), maxExtents.z - minExtents.z);
    	// glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, -(maxExtents.z - minExtents.z)/2.f, maxExtents.z - minExtents.z) / 2.f;
    	// glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.f, maxExtents.z - minExtents.z);

		cascadeDistances[i] = (nearClip + splitDist * clipRange) * -1.0f;

    	viewProjMats[i] = lightOrthoMatrix * lightViewMatrix;
	}
}

GPUShadowPassData* shadowPass(VulkanBackend& backend, RenderGraph& graph, Scene& scene, int cascadeCount) 
{
    RenderPass pass = {
        .debugName = "shadow pass",
        .pipeline = std::optional<RenderPass::Pipeline>(RenderPass::Pipeline())
    };
    pass.pipeline->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("simple_shadowpass.vert.glsl"));
    std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("empty.frag.glsl"));
    if (!vertexShader || !fragmentShader)
    {
        return {0};
    }

    VkPushConstantRange meshPushConstantRange = vkutil::init::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(ShadowPassPushConstants));
    // VkDescriptorSetLayout descriptors[] = {backend.sceneDescriptorSetLayout, backend.bindlessTexDescLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(nullptr, 0, &meshPushConstantRange, 1);
    VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pass.pipeline->pipelineLayout));

    pass.pipeline->pipeline = PipelineBuilder()
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

    // Here we should set up the resources in the rendergraph
    InternalShadowPassData* internalData = new InternalShadowPassData();
    GPUShadowPassData* gpuData = new GPUShadowPassData();

    gpuData->shadowMap = {0};
    constexpr int shadowMapSize = 1024;
    gpuData->shadowMap.extent = { cascadeCount * shadowMapSize, shadowMapSize, 1 };
    gpuData->shadowMap = backend.allocateImage(
        vkutil::init::imageCreateInfo(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, gpuData->shadowMap.extent),
        VMA_MEMORY_USAGE_GPU_ONLY,
        0, // NOTE: this might cause issues
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    
    VkRenderingAttachmentInfo* depthAttachmentInfo = new VkRenderingAttachmentInfo();
    *depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(gpuData->shadowMap.view);
    pass.renderingInfo = vkutil::init::renderingInfo({gpuData->shadowMap.extent.width, gpuData->shadowMap.extent.height}, nullptr, 0, depthAttachmentInfo);

    pass.draw = [&, internalData, gpuData, cascadeCount](VkCommandBuffer cmd, RenderPass& p) {
        TracyVkZone(backend.currentFrame().tracyCtx, backend.currentFrame().tracyCmdBuffer, "CSM pass");
        // NOTE: temporarily duplicated between this and basepass
        if (!internalData->initialized) 
        {
            internalData->initialized = true;

            // Vertex + index buffers
            const uint32_t vertexBufferSize = scene.vertexData.size() * sizeof(decltype(scene.vertexData)::value_type); 
            const uint32_t indexBufferSize = scene.indices.size() * sizeof(decltype(scene.indices)::value_type);

            std::println("Vert count: {}, element size: {}, total size: {}", scene.vertexData.size(), sizeof(decltype(scene.vertexData)::value_type), vertexBufferSize);
            std::println("Index count: {}, element size: {}, total size: {}", scene.indices.size(), sizeof(decltype(scene.indices)::value_type), indexBufferSize);

            auto info = vkutil::init::bufferCreateInfo(vertexBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            internalData->vertexDataBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            info = vkutil::init::bufferCreateInfo(indexBufferSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            internalData->indexBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_GPU_ONLY,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            backend.copyBufferWithStaging((void*)scene.vertexData.data(), vertexBufferSize, internalData->vertexDataBuffer.buffer);
            backend.copyBufferWithStaging((void*)scene.indices.data(), indexBufferSize, internalData->indexBuffer.buffer);

            // Indirect stuff
            info = vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * scene.meshes.size(),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            internalData->indirectCommands = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        	std::vector<VkDrawIndexedIndirectCommand> indirectCmds;
        	indirectCmds.reserve(scene.meshes.size());
        	for (int i = 0; i < scene.meshes.size(); ++i)
        	{
        	    auto& mesh = scene.meshes[i];
                VkDrawIndexedIndirectCommand c{};
            	c.instanceCount = 1;
            	c.firstInstance = i;
            	c.firstIndex = mesh.indexOffset;
            	c.indexCount = mesh.indexCount;
                indirectCmds.push_back(c);
        	}

            backend.copyBufferWithStaging((void*)indirectCmds.data(), sizeof(VkDrawIndexedIndirectCommand) * indirectCmds.size(),
                                           internalData->indirectCommands.buffer);

            // info = vkutil::init::bufferCreateInfo(vertexBufferSize,
            //     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
            //     VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
            //     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            // data->lightViewProjMatrices = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            //     VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            info = vkutil::init::bufferCreateInfo(sizeof(ShadowPassData),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            gpuData->shadowMapData = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }

        VkBufferDeviceAddressInfo vertexAddressInfo{ 
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, 
            .buffer = internalData->vertexDataBuffer.buffer 
        };
        VkBufferDeviceAddressInfo dataAddressInfo{ 
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, 
            .buffer = gpuData->shadowMapData.buffer 
        };

    	vkCmdBindIndexBuffer(cmd, internalData->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = shadowMapSize,
            .height = shadowMapSize,
            .maxDepth = 1.f
        };
        VkRect2D scissor = {
            .offset = VkOffset2D{0, 0},
            .extent = VkExtent2D{shadowMapSize, shadowMapSize}
        };

        static bool open = true;
        static int cascadeCount = 4;
        static float cascadeSplitLambda = 0.8f;
        if (ImGui::Begin("Render debug", &open))
        {
            if (ImGui::CollapsingHeader("CSM shadows"))
            {
                ImGui::SliderInt("Cascade count", &cascadeCount, 4, 4);
                ImGui::SliderFloat("Cascade split lambda", &cascadeSplitLambda, 0.001f, 2.f);
            }
        }
        ImGui::End();

        ShadowPassData data;
        data.cascadeCount = cascadeCount;
        csmLightViewProjMats(data.lightViewProjMatrices, data.cascadeDistances, cascadeCount, scene.mainCamera.view(), scene.mainCamera.proj(), scene.lightDir,
            scene.mainCamera.nearClippingPlaneDist, scene.mainCamera.farClippingPlaneDist, cascadeSplitLambda);
        for (int i = 0; i < cascadeCount; ++i)
        {
            data.invLightViewProjMatrices[i] = glm::inverse(data.lightViewProjMatrices[i]);
        }
        backend.copyBufferWithStaging((void*)&data, sizeof(ShadowPassData), gpuData->shadowMapData.buffer);

    	for (int i = 0; i < cascadeCount; ++i)
    	{
            ShadowPassPushConstants pushConstants 
            {
                .vertexBufferAddr = vkGetBufferDeviceAddress(backend.device, &vertexAddressInfo),
                .dataBufferAddr = vkGetBufferDeviceAddress(backend.device, &dataAddressInfo),
                .cascade = i
            };
            vkCmdPushConstants(cmd, p.pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPassPushConstants), &pushConstants);

            viewport.x = i * shadowMapSize;
        	scissor.offset.x = viewport.x;
        	vkCmdSetViewport(cmd, 0, 1, &viewport);
        	vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdDrawIndexedIndirect(cmd, internalData->indirectCommands.buffer, 0, scene.meshes.size(), sizeof(VkDrawIndexedIndirectCommand));
    	}
    };

    graph.renderpasses.push_back(pass);
    return gpuData;
}
