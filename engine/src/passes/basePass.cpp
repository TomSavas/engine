#include "passes/passes.h"
#include "scene.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"

#include <vulkan/vulkan_core.h>
#include <glm/gtx/transform.hpp>

struct BasePassData
{
    AllocatedBuffer perModelDataBuffer;
    AllocatedBuffer vertexDataBuffer;
    AllocatedBuffer indexBuffer;    
    AllocatedBuffer indirectCommands;
    bool initialized = false;
};

AllocatedBuffer allocBuf(VmaAllocator allocator, VkBufferCreateInfo info, VmaMemoryUsage usage, VmaAllocationCreateFlags flags, VkMemoryPropertyFlags requiredFlags)
{
    AllocatedBuffer buffer;
    
    VmaAllocationCreateInfo allocInfo
    {
        .flags = flags,
        .usage = usage,
        .requiredFlags = requiredFlags,    
    };
    VK_CHECK(vmaCreateBuffer(allocator, &info, &allocInfo, &buffer.buffer, &buffer.allocation, nullptr));

    return buffer;
}

std::optional<RenderPass> basePass(VulkanBackend& backend, Scene& scene) {
    RenderPass pass;
    pass.debugName = "base pass";
    pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("mesh.vert.glsl"));
    std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("mesh.frag.glsl"));
    if (!vertexShader || !fragmentShader)
    {
        return std::optional<RenderPass>();
    }

    VkPushConstantRange meshPushConstantRange = vkutil::init::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstants));
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(&backend.sceneDescriptorSetLayout, 1, &meshPushConstantRange, 1);
    VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pass.pipelineLayout));

    pass.pipeline = PipelineBuilder()
        .shaders((*vertexShader)->module, (*fragmentShader)->module)
        .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .polyMode(VK_POLYGON_MODE_FILL)
        .cullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .disableMultisampling()
        .enableAlphaBlending()
        .colorAttachmentFormat(backend.backbufferImage.format)
        .depthFormat(backend.depthImage.format)
        .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
        .build(backend.device, pass.pipelineLayout);

    // Here we should set up the resources in the rendergraph

    BasePassData* basePassData = new BasePassData();
    pass.draw = [&, basePassData](VkCommandBuffer cmd, RenderPass& p) {
        if (!basePassData->initialized) 
        {
            basePassData->initialized = true;

            // Indirect commands buffer
            auto info = vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            basePassData->indirectCommands = allocBuf(backend.allocator, info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            // Allocate buffers
            const uint32_t vertexBufferSize = scene.vertexData.size() * sizeof(decltype(scene.vertexData)::value_type); 
            const uint32_t indexBufferSize = scene.indices.size() * sizeof(decltype(scene.indices)::value_type);

            std::println("Vert count: {}, element size: {}, total size: {}", scene.vertexData.size(), sizeof(decltype(scene.vertexData)::value_type), vertexBufferSize);
            std::println("Index count: {}, element size: {}, total size: {}", scene.indices.size(), sizeof(decltype(scene.indices)::value_type), indexBufferSize);

            info = vkutil::init::bufferCreateInfo(vertexBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            basePassData->vertexDataBuffer = allocBuf(backend.allocator, info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            info = vkutil::init::bufferCreateInfo(indexBufferSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            basePassData->indexBuffer = allocBuf(backend.allocator, info, VMA_MEMORY_USAGE_GPU_ONLY,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            backend.copyBufferWithStaging((void*)scene.vertexData.data(), vertexBufferSize, basePassData->vertexDataBuffer.buffer);
            backend.copyBufferWithStaging((void*)scene.indices.data(), indexBufferSize, basePassData->indexBuffer.buffer);
        }

        // Add object data on the fly

        // Generate draw indirect commands on the fly
        // TODO: CPU culling, later add option for GPU culling
        VkDrawIndexedIndirectCommand indirectCmd{};
    	indirectCmd.instanceCount = 1;
    	indirectCmd.firstInstance = 0;
    	indirectCmd.firstIndex = 0;
    	indirectCmd.indexCount = scene.indices.size();

        backend.copyBufferWithStaging((void*)&indirectCmd, sizeof(VkDrawIndexedIndirectCommand), basePassData->indirectCommands.buffer);

        VkBufferDeviceAddressInfo deviceAddressInfo{ 
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, 
            .buffer = basePassData->vertexDataBuffer.buffer 
        };
        PushConstants pushConstants 
        {
            // .model = glm::mat4(1.f), //SRT 
            .model = glm::translate(glm::vec3(3.f, 3.f, 3.f)), //SRT 
            .color = glm::vec4(1.f, 0.f, 0.f, 1.f),
            .vertexBufferAddr = vkGetBufferDeviceAddress(backend.device, &deviceAddressInfo)
        };
        vkCmdPushConstants(cmd, p.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);
    	vkCmdBindIndexBuffer(cmd, basePassData->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        for (int i = 0; i < 1; i++)
        {
            vkCmdDrawIndexedIndirect(cmd, basePassData->indirectCommands.buffer, i * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
        }
    };

    return pass;
}
