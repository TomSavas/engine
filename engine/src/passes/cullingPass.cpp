#include <vulkan/vulkan_core.h>

#include <glm/gtx/transform.hpp>

#include "passes/culling.h"
#include "rhi/renderpass.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"
#include "scene.h"

auto insideCameraFrustum(const glm::vec3 aabbMin, const glm::vec3 aabbMax,
    const std::array<glm::vec4, 6>& frustumPlanes)
    -> bool
{
    for (u8 i = 0; i < 6; ++i)
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

auto initCulling(VulkanBackend& backend) -> std::optional<GeometryCulling>
{
    // FIXME: we should not be hardcoding the mesh count
    const auto info = vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * 1000,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    return GeometryCulling{
        .culledDraws = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
}

auto cpuFrustumCullingPass(std::optional<GeometryCulling>& geometryCulling, VulkanBackend& backend, RenderGraph& graph)
    -> CullingPassRenderGraphData
{
    if (!geometryCulling)
    {
        geometryCulling = initCulling(backend);
    }

    auto& pass = createPass(graph);
    pass.pass.debugName = "CPU frustum culling pass";

    CullingPassRenderGraphData data = {};
    data.culledDraws = importResource<Buffer>(graph, pass, &geometryCulling->culledDraws.buffer);

    pass.pass.draw = [data, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass&, Scene& scene)
    {
        ZoneScopedN("culling");

        {
            ZoneScopedN("Real culling");


        const auto view = glm::inverse(
            glm::translate(glm::mat4(1.f), scene.mainCamera.position) * scene.mainCamera.rotation);
        const auto projection = glm::perspectiveFov<f32>(scene.mainCamera.verticalFov,
            backend.backbufferImage.extent.width, backend.backbufferImage.extent.height,
            scene.mainCamera.nearClippingPlaneDist, scene.mainCamera.farClippingPlaneDist);
        const auto viewProj = projection * view;
        const auto viewProjTranspose = glm::transpose(viewProj);
        const std::array frustumPlanes = {
            (viewProjTranspose[3] + viewProjTranspose[0]),
            (viewProjTranspose[3] - viewProjTranspose[0]),
            (viewProjTranspose[3] + viewProjTranspose[1]),
            (viewProjTranspose[3] - viewProjTranspose[1]),
            (viewProjTranspose[3] + viewProjTranspose[2]),
            (viewProjTranspose[3] - viewProjTranspose[2]),
        };

        // TODO: shouldn't recalculate this unecessarily
        //std::vector<VkDrawIndexedIndirectCommand> indirectCmds;
        //indirectCmds.reserve(scene.meshes.size());
        //for (u32 i = 0; i < scene.meshes.size(); ++i)
        //{
        //    const auto& mesh = scene.meshes[i];
        //    const u32 instanceCount = insideCameraFrustum(mesh.aabbMin, mesh.aabbMax, frustumPlanes) ? 1 : 0;
        //    VkDrawIndexedIndirectCommand command = {
        //        .indexCount = static_cast<u32>(mesh.indexCount),
        //        .instanceCount = instanceCount,
        //        .firstIndex = static_cast<u32>(mesh.indexOffset),
        //        .vertexOffset = 0,
        //        .firstInstance = i
        //    };
        //    indirectCmds.push_back(command);
        //}

        std::vector<VkDrawIndexedIndirectCommand> indirectCmds;
        indirectCmds.reserve(scene.meshCount);
        u32 i = 0;
        for (auto& mesh : scene.meshes)
        {
            for (auto& instance : mesh.second.instances)
            {
                //const u32 instanceCount = insideCameraFrustum(instance.aabbMin, instance.aabbMax, frustumPlanes) ? 1 : 0;
                const u32 instanceCount = 1;
                VkDrawIndexedIndirectCommand command = {
                    .indexCount = static_cast<u32>(mesh.second.indexCount),
                    .instanceCount = instanceCount,
                    .firstIndex = static_cast<u32>(mesh.second.indexOffset),
                    .vertexOffset = 0,
                    .firstInstance = i++
                };
                indirectCmds.push_back(command);
            }
        }

        backend.copyBufferWithStaging(indirectCmds.data(), sizeof(VkDrawIndexedIndirectCommand) * indirectCmds.size(),
            *getResource<Buffer>(graph, data.culledDraws));
        }
    };

    return data;
}
