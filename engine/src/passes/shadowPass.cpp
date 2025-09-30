#include "passes/shadows.h"

#include "rhi/renderpass.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"
#include "scene.h"

#include <glm/glm.hpp>
#include <optional>

struct CascadeParams
{
    glm::mat4 lightViewProjMatrices[4];
    glm::mat4 invLightViewProjMatrices[4];
    glm::vec4 cascadeDistances[4];
    u32 cascadeCount;
};

auto frustumCornersInWorldSpace(glm::mat4 invViewProj) -> std::array<glm::vec3, 8>
{
    std::array frustumCorners = {
        glm::vec3(-1.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, -1.0f, 0.0f),
        glm::vec3(-1.0f, -1.0f, 0.0f),
        glm::vec3(-1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, -1.0f, 1.0f),
        glm::vec3(-1.0f, -1.0f, 1.0f),
    };

    for (u32 i = 0; i < 8; i++)
    {
        const auto invCorner = invViewProj * glm::vec4(frustumCorners[i], 1.0f);
        frustumCorners[i] = invCorner / invCorner.w;
    }

    return frustumCorners;
}

void csmLightViewProjMats(glm::mat4* viewProjMats, glm::vec4* cascadeDistances, i32 cascadeCount, glm::mat4 view,
    glm::mat4 proj, glm::vec3 lightDirr, f32 nearClip, f32 farClip, f32 cascadeSplitLambda, f32 resolution)
{
    // f32 cascadeSplitLambda = 0.8f;
    f32 cascadeSplits[cascadeCount];

    f32 clipRange = farClip - nearClip;

    f32 minZ = nearClip;
    f32 maxZ = nearClip + clipRange;

    f32 range = maxZ - minZ;
    f32 ratio = maxZ / minZ;

    // Calculate split depths based on view camera frustum
    // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
    for (u32 i = 0; i < cascadeCount; i++)
    {
        f32 p = (i + 1) / static_cast<f32>(cascadeCount);
        f32 log = minZ * std::pow(ratio, p);
        f32 uniform = minZ + range * p;
        f32 d = cascadeSplitLambda * (log - uniform) + uniform;
        // cascadeDistances[i] = (d - nearClip) / clipRange;
        cascadeSplits[i] = (d - nearClip) / clipRange;
    }

    f32 lastSplitDist = 0.f;
    glm::mat4 invViewProj = glm::inverse(proj * view);
    for (i32 i = 0; i < cascadeCount; i++)
    {
        std::array<glm::vec3, 8> frustumCorners = frustumCornersInWorldSpace(invViewProj);

        f32 splitDist = cascadeSplits[i];
        for (u32 j = 0; j < 4; j++)
        {
            glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
            frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
            frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
        }
        lastSplitDist = cascadeSplits[i];

        glm::vec3 frustumCenter = glm::vec3(0.0f);
        for (u32 j = 0; j < 8; j++)
        {
            frustumCenter += frustumCorners[j];
        }
        frustumCenter /= 8.0f;

        f32 radius = 0.0f;
        for (u32 j = 0; j < 8; j++)
        {
            f32 distance = glm::length(frustumCorners[j] - frustumCenter);
            radius = glm::max(radius, distance);
        }
        //radius = std::ceil(radius / 2.0f) * 2.0f;
        radius = std::ceil(radius * 16.0f) / 16.0f;

        glm::vec3 lightDir = normalize(lightDirr); // NOTE: convert lightDir into light pos

        glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - (lightDir * radius), frustumCenter,
            glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightOrthoMatrix = glm::ortho(-radius, radius, -radius, radius,
            -radius * 2, radius * 2);
        viewProjMats[i] = lightOrthoMatrix * lightViewMatrix;

        glm::vec4 lightSpaceOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        lightSpaceOrigin = viewProjMats[i] * lightSpaceOrigin;
        lightSpaceOrigin = lightSpaceOrigin * (resolution / 2.0f); // Range [-resolution/2; resolution/2]

        glm::vec4 roundedOrigin = glm::round(lightSpaceOrigin);
        glm::vec4 roundOffset = roundedOrigin - lightSpaceOrigin;
        roundOffset = roundOffset * (2.0f / resolution);
        roundOffset.z = 0.0f;
        roundOffset.w = 0.0f;

        glm::mat4 shadowProj = viewProjMats[i];
        shadowProj[3] += roundOffset;
        viewProjMats[i] = shadowProj;

        cascadeDistances[i] = glm::vec4((nearClip + splitDist * clipRange) * -1.0f);
    }
}

auto csmCascadeParams(u32 cascadeCount, Camera& camera, glm::vec3 lightDir, f32 cascadeSplitLambda, f32 resolution)
    -> CascadeParams
{
    CascadeParams cascadeData = {
        .cascadeCount = cascadeCount,
    };

    csmLightViewProjMats(cascadeData.lightViewProjMatrices, cascadeData.cascadeDistances, cascadeCount,
        camera.view(), camera.proj(), lightDir, camera.nearClippingPlaneDist,
        camera.farClippingPlaneDist, 0.5, resolution);
    for (u32 i = 0; i < cascadeCount; ++i)
    {
        cascadeData.invLightViewProjMatrices[i] = glm::inverse(cascadeData.lightViewProjMatrices[i]);
    }

    return cascadeData;
}

struct ShadowPushConstants
{
    VkDeviceAddress vertexBufferAddr;
    VkDeviceAddress cascadeDataAddr;
    VkDeviceAddress perModelDataBufferAddr;
    u32 cascade;
};

auto initCsm(VulkanBackend& backend, u32 cascadeCount) -> ShadowRenderer
{
    const auto info = vkutil::init::bufferCreateInfo(
        sizeof(CascadeParams), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    const auto cascadeParams = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    constexpr u32 shadowMapSize = 4096;
    const auto shadowMapImage = backend.allocateImage(
        vkutil::init::imageCreateInfo(VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            {cascadeCount * shadowMapSize, shadowMapSize, 1}),
        VMA_MEMORY_USAGE_GPU_ONLY,
        0,  // NOTE: this might cause issues
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    return ShadowRenderer{
        .pipeline = PipelineBuilder(backend)
            .addPushConstants({
                VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                    .offset = 0,
                    .size = sizeof(ShadowPushConstants)
                }
            })
            .addShader(SHADER_PATH("cascaded_shadows.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(SHADER_PATH("empty.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
            .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .polyMode(VK_POLYGON_MODE_FILL)
            .cullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)  // Front face!
            .disableMultisampling()
            .disableBlending()
            .depthFormat(shadowMapImage.format)
            .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
            .setDepthClamp(true)
            .addViewportScissorDynamicStates()
            .build(),
        .cascadeParams = cascadeParams,
        .shadowMap = backend.bindlessResources->addTexture(
            Texture{
                .image = shadowMapImage,
                .view = shadowMapImage.view,
                .mipCount = 1,
            }
        ),
    };
}

auto csmPass(std::optional<ShadowRenderer>& shadowRenderer, VulkanBackend& backend, RenderGraph& graph, u8 cascadeCount)
    ->ShadowPassRenderGraphData
{
    if (!shadowRenderer)
    {
        shadowRenderer = initCsm(backend, cascadeCount);
    }

    auto& pass = createPass(graph);
    pass.pass.debugName = "CSM pass";
    pass.pass.pipeline = shadowRenderer->pipeline;

    ShadowPassRenderGraphData data = {
        .shadowMap = writeResource<BindlessTexture>(graph, pass,
            importResource(graph, pass, &shadowRenderer->shadowMap), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL),
        .cascadeParams = writeResource<Buffer>(graph, pass,
            importResource(graph, pass, &shadowRenderer->cascadeParams.buffer))
    };

    pass.pass.beginRendering = [data, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    {
        const auto shadowMap = backend.bindlessResources->getTexture(
            *getResource<BindlessTexture>(graph, data.shadowMap));
        const VkExtent2D size = {
            .width = shadowMap.image.extent.width,
            .height = shadowMap.image.extent.height
        };

        auto depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(shadowMap.view);
        auto renderingInfo = vkutil::init::renderingInfo(size, nullptr, 0, &depthAttachmentInfo);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [data, cascadeCount, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass,
        Scene& scene)
    {
        ZoneScopedCpuGpuAuto("CSM pass", backend.currentFrame());

        const auto shadowMap = backend.bindlessResources->getTexture(
            *getResource<BindlessTexture>(graph, data.shadowMap));
        const auto singleCascadeSize = shadowMap.image.extent.height;

        auto cascadeParams = csmCascadeParams(cascadeCount, scene.mainCamera, scene.lightDir, 0.5,
            static_cast<f32>(singleCascadeSize));
        auto cascadeParamBuffer = *getResource<Buffer>(graph, data.cascadeParams);
        backend.copyBufferWithStaging(&cascadeParams, sizeof(cascadeParams), cascadeParamBuffer);

        ShadowPushConstants pushConstants{
            .vertexBufferAddr = backend.getBufferDeviceAddress(scene.vertexBuffer.buffer),
            .cascadeDataAddr = backend.getBufferDeviceAddress(cascadeParamBuffer),
            .perModelDataBufferAddr = backend.getBufferDeviceAddress(scene.perModelBuffer.buffer),
        };

        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<f32>(singleCascadeSize),
            .height = static_cast<f32>(singleCascadeSize),
            .maxDepth = 1.f
        };
        VkRect2D scissor = {
            .offset = VkOffset2D{0, 0},
            .extent = VkExtent2D{singleCascadeSize, singleCascadeSize}
        };

        vkCmdBindIndexBuffer(cmd, scene.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        for (u32 i = 0; i < cascadeCount; ++i)
        {
            pushConstants.cascade = i;
            vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants),
                &pushConstants);

            viewport.x = static_cast<f32>(i) * static_cast<f32>(singleCascadeSize);
            scissor.offset.x = viewport.x;
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdDrawIndexedIndirect(cmd, scene.indirectCommands.buffer, 0, scene.meshes.size(),
                sizeof(VkDrawIndexedIndirectCommand));
        }
    };

    return data;
}