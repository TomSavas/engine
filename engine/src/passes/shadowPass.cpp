
#include "passes/passes.h"
#include "scene.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"

#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

struct ShadowPassData 
{
    AllocatedImage shadowMap;  

    // For the time being duplicate between this and basepass
    AllocatedBuffer vertexDataBuffer;
    AllocatedBuffer indexBuffer;    
    // This should be kept in the scene most likely
    AllocatedBuffer indirectCommands;
    bool initialized = false;
};

struct ShadowPassPushConstants
{
    glm::mat4 lightViewProj;
    VkDeviceAddress vertexBufferAddr;
};

std::array<glm::vec4, 8> frustumCornersWorldSpace(glm::mat4 invViewProj)
{
    std::array<glm::vec4, 8> corners;
    for (int x = 0; x <= 1; ++x)
    {
        for (int y = 0; y <= 1; ++y)
        {
            for (int z = 0; z <= 1; ++z)
            {
                glm::vec4 ndcPoint = {
                    2.f * x - 1.f,
                    2.f * y - 1.f,
                    // (float)z,
                    2.f * z - 1.f,
                    1.f
                };
                glm::vec4 world = invViewProj * ndcPoint;
                world = world / world.w;
                corners[x + y*2 + z*4] = world;
            }
        }
    }

    return corners;
}

glm::mat4 frustumEnclosingLightViewProj(glm::mat4 view, glm::mat4 proj, glm::vec3 lightDir)
{
    // auto frustumCorners = frustumCornersWorldSpace(glm::inverse(proj * view));
    
    // glm::vec3 center = glm::vec3(0, 0, 0);
    // for (const glm::vec4& v : frustumCorners)
    // {
    //     center += glm::vec3(v);
    // }
    // center /= frustumCorners.size();
    
    // glm::mat4 lightView = glm::lookAt(
    //     center + lightDir,
    //     center,
    //     glm::vec3(0.0f, 1.0f, 0.0f)
    // );

    // float minX = std::numeric_limits<float>::max();
    // float maxX = std::numeric_limits<float>::lowest();
    // float minY = std::numeric_limits<float>::max();
    // float maxY = std::numeric_limits<float>::lowest();
    // float minZ = std::numeric_limits<float>::max();
    // float maxZ = std::numeric_limits<float>::lowest();
    // for (const glm::vec4& v : frustumCorners)
    // {
    //     const auto trf = lightView * v;
    //     minX = std::min(minX, trf.x);
    //     maxX = std::max(maxX, trf.x);
    //     minY = std::min(minY, trf.y);
    //     maxY = std::max(maxY, trf.y);
    //     minZ = std::min(minZ, trf.z);
    //     maxZ = std::max(maxZ, trf.z);
    // }

    // // Tune this parameter according to the scene
    // constexpr float zMult = 1.f;
    // if (minZ < 0)
    // {
    //     minZ *= zMult;
    // }
    // else
    // {
    //     minZ /= zMult;
    // }
    // if (maxZ < 0)
    // {
    //     maxZ /= zMult;
    // }
    // else
    // {
    //     maxZ *= zMult;
    // }
   
    // const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    // return lightProjection * lightView;

    glm::vec3 frustumCorners[8] = {
		glm::vec3(-1.0f,  1.0f, 0.0f),
		glm::vec3( 1.0f,  1.0f, 0.0f),
		glm::vec3( 1.0f, -1.0f, 0.0f),
		glm::vec3(-1.0f, -1.0f, 0.0f),
		glm::vec3(-1.0f,  1.0f,  1.0f),
		glm::vec3( 1.0f,  1.0f,  1.0f),
		glm::vec3( 1.0f, -1.0f,  1.0f),
		glm::vec3(-1.0f, -1.0f,  1.0f),
	};

	// Project frustum corners into world space
	glm::mat4 invCam = glm::inverse(proj * view);
	for (uint32_t j = 0; j < 8; j++) {
		glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
		frustumCorners[j] = invCorner / invCorner.w;
	}

	// for (uint32_t j = 0; j < 4; j++) {
	// 	glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
	// 	frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
	// 	frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
	// }

	// Get frustum center
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
	glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

	return lightOrthoMatrix * lightViewMatrix;
}

AllocatedImage shadowPass(VulkanBackend& backend, RenderGraph& graph, Scene& scene) 
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
        .build(backend.device, pass.pipeline->pipelineLayout);

    // Here we should set up the resources in the rendergraph
    ShadowPassData* data = new ShadowPassData();

    data->shadowMap = {0};
    data->shadowMap.extent = { 1024, 1024, 1 };
    data->shadowMap = backend.allocateImage(
        vkutil::init::imageCreateInfo(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, data->shadowMap.extent),
        VMA_MEMORY_USAGE_GPU_ONLY,
        0, // NOTE: this might cause issues
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    
    VkRenderingAttachmentInfo* depthAttachmentInfo = new VkRenderingAttachmentInfo();
    *depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(data->shadowMap.view);
    pass.renderingInfo = vkutil::init::renderingInfo({data->shadowMap.extent.width, data->shadowMap.extent.height}, nullptr, 0, depthAttachmentInfo);

    pass.draw = [&, data](VkCommandBuffer cmd, RenderPass& p) {
        // NOTE: temporarily duplicated between this and basepass
        if (!data->initialized) 
        {
            data->initialized = true;

            // Vertex + index buffers
            const uint32_t vertexBufferSize = scene.vertexData.size() * sizeof(decltype(scene.vertexData)::value_type); 
            const uint32_t indexBufferSize = scene.indices.size() * sizeof(decltype(scene.indices)::value_type);

            std::println("Vert count: {}, element size: {}, total size: {}", scene.vertexData.size(), sizeof(decltype(scene.vertexData)::value_type), vertexBufferSize);
            std::println("Index count: {}, element size: {}, total size: {}", scene.indices.size(), sizeof(decltype(scene.indices)::value_type), indexBufferSize);

            auto info = vkutil::init::bufferCreateInfo(vertexBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            data->vertexDataBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            info = vkutil::init::bufferCreateInfo(indexBufferSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            data->indexBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_GPU_ONLY,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            backend.copyBufferWithStaging((void*)scene.vertexData.data(), vertexBufferSize, data->vertexDataBuffer.buffer);
            backend.copyBufferWithStaging((void*)scene.indices.data(), indexBufferSize, data->indexBuffer.buffer);

            // Indirect stuff
            info = vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * scene.meshes.size(),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            data->indirectCommands = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
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
                                           data->indirectCommands.buffer);
        }

        // Generate draw indirect commands on the fly
        // TODO: CPU culling, later add option for GPU culling

        VkBufferDeviceAddressInfo vertexAddressInfo{ 
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, 
            .buffer = data->vertexDataBuffer.buffer 
        };

        ShadowPassPushConstants pushConstants 
        {
            .lightViewProj = frustumEnclosingLightViewProj(scene.mainCamera.view(), scene.mainCamera.proj(), scene.lightDir),
            .vertexBufferAddr = vkGetBufferDeviceAddress(backend.device, &vertexAddressInfo),
        };
        vkCmdPushConstants(cmd, p.pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPassPushConstants), &pushConstants);
    	vkCmdBindIndexBuffer(cmd, data->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexedIndirect(cmd, data->indirectCommands.buffer, 0, scene.meshes.size(), sizeof(VkDrawIndexedIndirectCommand));
    };

    graph.renderpasses.push_back(pass);
    return data->shadowMap;
}
