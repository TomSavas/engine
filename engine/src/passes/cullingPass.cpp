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

auto initCulling(VulkanBackend& backend) -> GeometryCulling
{
    // FIXME: we should not be hardcoding the mesh count
    const auto info = vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * 1000,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    return GeometryCulling{
        .culledDraws = backend.allocateBuffer("Culled draw commands", info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
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
    data.culledDraws = writeResource<Buffer>(graph, pass, importResource(graph, pass, &geometryCulling->culledDraws));

    pass.pass.draw = [data, &backend](const RenderContext& ctx, RenderPass& pass)
    {
        ZoneScopedN("CPU Frustum culling");
        {

            const auto view = glm::inverse(
                glm::translate(glm::mat4(1.f), ctx.scene.mainCamera.position) * ctx.scene.mainCamera.rotation);
            const auto projection = glm::perspectiveFov<f32>(ctx.scene.mainCamera.verticalFov,
                backend.scaledResolution.x, backend.scaledResolution.y,
                ctx.scene.mainCamera.nearClippingPlaneDist, ctx.scene.mainCamera.farClippingPlaneDist);
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

            std::vector<VkDrawIndexedIndirectCommand> indirectCmds;
            indirectCmds.reserve(ctx.scene.meshCount);
            for (auto& mesh : ctx.scene.meshes)
            {
                for (auto& instance : mesh.second.instances)
                {
                    const u32 instanceCount = insideCameraFrustum(instance.aabbMin, instance.aabbMax, frustumPlanes) ? 1 : 0;
                    VkDrawIndexedIndirectCommand command = {
                        .indexCount = static_cast<u32>(mesh.second.indexCount),
                        .instanceCount = instanceCount,
                        .firstIndex = static_cast<u32>(mesh.second.indexOffset),
                        .vertexOffset = 0,
                        .firstInstance = 0,
                    };
                    indirectCmds.push_back(command);
                }
            }

            backend.copyBufferWithStaging(ctx.cmd, indirectCmds.data(), sizeof(VkDrawIndexedIndirectCommand) * indirectCmds.size(),
                getResource<Buffer>(ctx.graph, data.culledDraws)->buffer);
        }
    };

    return data;
}
