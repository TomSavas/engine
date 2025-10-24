#include "passes/sceneData.h"

#include <glm/vec4.hpp>
#include <optional>

#include "renderGraph.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/inits.h"
#include "scene.h"

auto sceneUploadPass(std::optional<SceneDataUploader>& sceneUploader, VulkanBackend& backend, RenderGraph& graph, Scene& scene)
    -> SceneUploadRenderGraphData
{
    auto& pass = createPass(graph);
    pass.pass.debugName = std::format("Scene data upload pass");

    // TODO: will get replaced by createResource
    if (!sceneUploader)
    {
        sceneUploader = SceneDataUploader();
        auto modelData = gatherModelData(scene);

        u32 perModelBufferSize = modelData.size() * sizeof(decltype(modelData)::value_type);
        perModelBufferSize = perModelBufferSize == 0 ? 8 : perModelBufferSize;
        auto info = vkutil::init::bufferCreateInfo(perModelBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        sceneUploader->perModelBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_GPU_ONLY,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        u32 modelDataSize = sizeof(VkDrawIndexedIndirectCommand) * modelData.size();
        modelDataSize = modelDataSize == 0 ? 8 : modelDataSize;
        info = vkutil::init::bufferCreateInfo(modelDataSize,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        sceneUploader->allDraws = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    auto data = SceneUploadRenderGraphData
    {
        .allDraws = writeResource<Buffer>(graph, pass, importResource(graph, pass, &sceneUploader->allDraws)),
        // .lightList = writeResource<Buffer>(graph, pass, importResource(graph, pass, &scene.perModelBuffer.buffer)),
        .perModelData = writeResource<Buffer>(graph, pass, importResource(graph, pass, &sceneUploader->perModelBuffer))
    };

    pass.pass.draw = [&backend, data](const RenderContext& ctx, RenderPass&)
    {
        // sceneUniforms.cameraPos = glm::vec4(scene.activeCamera->position, 1.f);
        // sceneUniforms.view = scene.activeCamera->view();
        // sceneUniforms.projection = scene.activeCamera->proj();
        // sceneUniforms.lightDir = glm::vec4(scene.lightDir, 5.f);
        // static f64 time = 0.f;
        // time += frame.stats.pastFrameDt;
        // sceneUniforms.time = glm::vec4(time);
        //
        // u8* dataOnGpu;
        // vmaMapMemory(allocator, sceneUniformBuffer.allocation, (void**)&dataOnGpu);
        // memcpy(dataOnGpu, &sceneUniforms, sizeof(sceneUniforms));
        // vmaUnmapMemory(allocator, sceneUniformBuffer.allocation);

        // Upload per model data

        auto modelData = gatherModelData(ctx.scene);
        // if (ctx.scene.drawListDirty)
        {

        }

        // if (ctx.scene.lightListDirty)
        {
            std::vector<VkDrawIndexedIndirectCommand> cmds;
            cmds.reserve(modelData.size());
            u32 i = 0;
            for (auto& mesh : ctx.scene.meshes)
            {
                for (auto& instance : mesh.second.instances)
                {
                    VkDrawIndexedIndirectCommand command = {
                        .indexCount = static_cast<u32>(mesh.second.indexCount),
                        .instanceCount = 1,
                        .firstIndex = static_cast<u32>(mesh.second.indexOffset),
                        .vertexOffset = 0,
                        .firstInstance = i++
                    };
                    cmds.push_back(command);
                }
            }

            backend.copyBufferWithStaging(cmds.data(), sizeof(VkDrawIndexedIndirectCommand) * cmds.size(), getResource<Buffer>(ctx.graph, data.allDraws)->buffer);
        }

        // if (ctx.scene.modelDataDirty)
        {
            backend.copyBufferWithStaging(modelData.data(), modelData.size() * sizeof(ModelData), getResource<Buffer>(ctx.graph, data.perModelData)->buffer);
        }
    };

    return data;
}
