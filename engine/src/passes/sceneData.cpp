#include "passes/sceneData.h"

#include "renderGraph.h"
#include "rhi/vulkan/backend.h"
#include "scene.h"

#include <glm/vec4.hpp>

#include <optional>

auto sceneUploadPass(std::optional<SceneDataUploader>& sceneUploader, VulkanBackend& backend, RenderGraph& graph)
    -> void
{
    auto& pass = createPass(graph);
    pass.pass.debugName = std::format("Scene data upload pass");

    // TODO: split into multiple

    pass.pass.draw = [&backend](const RenderContext& ctx, RenderPass& pass)
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
        backend.copyBufferWithStaging(modelData.data(), modelData.size() * sizeof(ModelData), ctx.scene.perModelBuffer.buffer);
    };
}
