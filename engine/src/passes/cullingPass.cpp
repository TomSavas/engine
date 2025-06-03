#include "passes/passes.h"

#include "passes/culling.h"
#include "scene.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"

#include <vulkan/vulkan_core.h>
#include <glm/gtx/transform.hpp>

bool insideCameraFrustum(const glm::vec3 aabbMin, const glm::vec3 aabbMax, const std::array<glm::vec4, 6>& frustumPlanes) 
{
    for (size_t i = 0; i < 6; ++i)
    {
        const glm::vec4& plane = frustumPlanes[i];
        if ((glm::dot(plane, glm::vec4(aabbMin.x, aabbMin.y, aabbMin.z, 1.0f)) < 0.0) &&
            (glm::dot(plane, glm::vec4(aabbMax.x, aabbMin.y, aabbMin.z, 1.0f)) < 0.0) &&
            (glm::dot(plane, glm::vec4(aabbMin.x, aabbMax.y, aabbMin.z, 1.0f)) < 0.0) &&
            (glm::dot(plane, glm::vec4(aabbMax.x, aabbMax.y, aabbMin.z, 1.0f)) < 0.0) &&
            (glm::dot(plane, glm::vec4(aabbMin.x, aabbMin.y, aabbMax.z, 1.0f)) < 0.0) &&
            (glm::dot(plane, glm::vec4(aabbMax.x, aabbMin.y, aabbMax.z, 1.0f)) < 0.0) &&
            (glm::dot(plane, glm::vec4(aabbMin.x, aabbMax.y, aabbMax.z, 1.0f)) < 0.0) &&
            (glm::dot(plane, glm::vec4(aabbMax.x, aabbMax.y, aabbMax.z, 1.0f)) < 0.0))
        {
            return false;
        }
    }

    return true;
}

std::optional<GeometryCulling> initCulling(RHIBackend& backend)
{
    GeometryCulling geometryCulling;
    
    auto info = vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * scene.meshes.size(),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    geometryCulling.culledDraws = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

CullingPassRenderGraphData cpuFrustumCullingPass(std::optional<GeometryCulling>& geometryCulling, RHIBackend& backend, RenderGraph& graph)
{
    if (geometryCulling)
    {
        geometryCulling = initCulling();
    }

    RenderPass& pass = createPass(graph);
    pass.debugName = "CPU frustum culling pass";
    // pass.pipeline = shadowRenderer.pipeline;
    // pass.pipeline->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    CullingPassRenderGraphData data;
    data.culledDraws = importResource<Buffer>(graph, pass, geometryCulling->culledDraws);

    pass.draw = [data](VkCommandBuffer, CompiledRenderGraph&, RenderPass&, Scene& scene)
    {
        const glm::mat4 view = glm::inverse(glm::translate(glm::mat4(1.f), scene.mainCamera.position) * scene.mainCamera.rotation);
        const glm::mat4 projection = glm::perspectiveFov<float>(scene.mainCamera.verticalFov, backend.backbufferImage.extent.width,
            backend.backbufferImage.extent.height, scene.mainCamera.nearClippingPlaneDist,
            scene.mainCamera.farClippingPlaneDist);
        const glm::mat4 viewProj = projection * view;
        const glm::mat4 viewProjTranspose = glm::transpose(viewProj);

        const std::array<glm::vec4, 6> frustumPlanes = {
            (viewProjTranspose[3] + viewProjTranspose[0]),
            (viewProjTranspose[3] - viewProjTranspose[0]),
            (viewProjTranspose[3] + viewProjTranspose[1]),
            (viewProjTranspose[3] - viewProjTranspose[1]),
            (viewProjTranspose[3] + viewProjTranspose[2]),
            (viewProjTranspose[3] - viewProjTranspose[2]),
        };        

        // TODO: shouldn't recalculate this unecessarily
    	std::vector<VkDrawIndexedIndirectCommand> indirectCmds;
    	indirectCmds.reserve(scene.meshes.size());
    	int culledCount = 0;
    	for (int i = 0; i < scene.meshes.size(); ++i)
    	{
    	    auto& mesh = scene.meshes[i];
    	    VkDrawIndexedIndirectCommand command =
	        {
    	        .indexCount = mesh.indexCount,
    	        .instanceCount = 1,
    	        .firstIndex = mesh.indexOffset,
    	        .vertexOffset = 0,
    	        .firstInstance = i
    	    } 
        	if (!insideCameraFrustum(mesh.aabbMin, mesh.aabbMax, frustumPlanes))
        	{
            	command.instanceCount = 0;
            	culledCount += 1;
        	}

            indirectCmds.push_back(command);
    	}

        backend.copyBufferWithStaging((void*)indirectCmds.data(), sizeof(VkDrawIndexedIndirectCommand) * indirectCmds.size(),
                                       draws.indirectCommands.buffer);
    }

    return data;
}
