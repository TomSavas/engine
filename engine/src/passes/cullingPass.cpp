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
    for (size_t i = 0; i < 6; ++i) {
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

AllocatedBuffer cullingPass(VulkanBackend& backend, RenderGraph& graph, Scene& scene) {
    RenderPass pass = {
        .debugName = "culling pass",
        .pipeline = {}
    };
    CulledDraws draws;
    auto info = vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * scene.meshes.size(),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    draws.indirectCommands = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // pass.setup = []() {
    //     if (draws.indirectCommands.buffer == VK_NULL_HANDLE)
    //     {
    //         // Indirect commands buffer
    //         auto info = vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * scene.meshes.size(),
    //             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    //         draws->indirectCommands = allocBuf(backend.allocator, info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    //             VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    //     }

    //     // TODO: should be something like:
    //     // draws->indirectCommands = graph.allocate<AllocatedBuffer>(allocInfo);
    //     // graph.writes(draws->indirectCommands);
    // };
    pass.draw = [&, draws](VkCommandBuffer cmd, RenderPass& p)
    {
        glm::mat4 view = glm::inverse(glm::translate(glm::mat4(1.f), scene.mainCamera.position) * scene.mainCamera.rotation);
        glm::mat4 projection = glm::perspectiveFov<float>(scene.mainCamera.verticalFov, backend.backbufferImage.extent.width, backend.backbufferImage.extent.height, scene.mainCamera.nearClippingPlaneDist, scene.mainCamera.farClippingPlaneDist);
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
            VkDrawIndexedIndirectCommand c{};
        	c.instanceCount = 1;
        	c.firstInstance = i;
        	c.firstIndex = mesh.indexOffset;
        	c.indexCount = mesh.indexCount;

        	if (!insideCameraFrustum(mesh.aabbMin, mesh.aabbMax, frustumPlanes))
        	{
            	c.instanceCount = 0;
            	culledCount++;
        	}

            indirectCmds.push_back(c);
    	}

        backend.copyBufferWithStaging((void*)indirectCmds.data(), sizeof(VkDrawIndexedIndirectCommand) * indirectCmds.size(),
                                       draws.indirectCommands.buffer);
    };

    graph.renderpasses.push_back(pass);
    return draws.indirectCommands;
}
