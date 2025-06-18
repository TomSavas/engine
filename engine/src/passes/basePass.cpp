#include "passes/passes.h"
#include "scene.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/renderpass.h"
#include "rhi/vulkan/pipeline_builder.h"
#include "rhi/vulkan/utils/buffer.h"
#include "rhi/vulkan/utils/inits.h"

#include "imgui.h"

#include <vulkan/vulkan_core.h>
#include <glm/gtx/transform.hpp>

#include "GLFW/glfw3.h"

// struct ModelData 
// {
//     // int albedoTex;
//     // int normalTex;
//     glm::vec4 textures;
//     glm::mat4 model;
// };

// struct BasePassData
// {
//     AllocatedBuffer bindlessTextureBuffer;
//     AllocatedBuffer perModelDataBuffer;

//     AllocatedBuffer vertexDataBuffer;
//     AllocatedBuffer indexBuffer;    
//     bool initialized = false;
// };

// struct BasePassRenderGraphData
// {
//     RenderGraphResource<int> shadowPassBindlessTex;
//     RenderGraphBufferResource shadowData;
//     RenderGraphBufferResource culledDraws;
// };

// void basePass(VulkanBackend& backend, RenderGraph& graph, AllocatedBuffer culledDraws, GPUShadowPassData* shadowPassData) 
// {
//     RenderPass pass;
//     pass.debugName = "base pass";
//     pass.pipeline = std::optional<RenderPass::Pipeline>(RenderPass::Pipeline{});
//     pass.pipeline->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

//     setup = [](BasePassRenderGraphData& data){
//         data.shadowData = graph.read(shadowData);
//         data.culledDraws = graph.read(culledDraws);

//         data.output = graph.output(graph.backbuffer);
//     };        

//     draw = [](const BasePassRenderGraphData& data){
//         PushConstants pushConstants 
//         {
//             .model = glm::mat4(1.f), //SRT 
//             .color = glm::vec4(0.f, 0.f, 0.f, 1.f),
//             .vertexBufferAddr = vkGetBufferDeviceAddress(backend.device, &vertexAddressInfo),
//             .perModelDataBufferAddr = vkGetBufferDeviceAddress(backend.device, &perModelDataAddressInfo),
//             .shadowData = vkGetBufferDeviceAddress(backend.device, &shadowDataAddressInfo),
//             .shadowMapIndex = scene.images.size()
//         };
//         vkCmdPushConstants(cmd, p.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pushConstants);
//     	vkCmdBindIndexBuffer(cmd, basePassData->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

//     	rc.drawIndexedIndirect(data.culledDraws.get());
//         // vkCmdDrawIndexedIndirect(cmd, culledDraws.buffer, 0, scene.meshes.size(), sizeof(VkDrawIndexedIndirectCommand));
//     };

//     graph.registerPass(pass);
// }


// void basePass(VulkanBackend& backend, RenderGraph& graph, Scene& scene, AllocatedBuffer culledDraws, GPUShadowPassData* shadowPassData) 
// {
//     RenderPass pass;
//     pass.debugName = "base pass";
//     pass.pipeline = std::optional<RenderPass::Pipeline>(RenderPass::Pipeline{});
//     pass.pipeline->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

//     std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("mesh.vert.glsl"));
//     std::optional<ShaderModule*> fragmentShader = backend.shaderModuleCache.loadModule(backend.device, SHADER_PATH("mesh.frag.glsl"));
//     if (!vertexShader || !fragmentShader)
//     {
//         // return std::optional<RenderPass>();
//         return;
//     }

//     VkPushConstantRange meshPushConstantRange = vkutil::init::pushConstantRange(VK_SHADER_STAGE_ALL, sizeof(PushConstants));
//     VkDescriptorSetLayout descriptors[] = {backend.sceneDescriptorSetLayout, backend.bindlessTexDescLayout};
//     VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(descriptors, 2, &meshPushConstantRange, 1);
//     VK_CHECK(vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &pass.pipeline->pipelineLayout));

//     pass.pipeline->pipeline = PipelineBuilder()
//         .shaders((*vertexShader)->module, (*fragmentShader)->module)
//         .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
//         .polyMode(VK_POLYGON_MODE_FILL)
//         .cullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
//         .disableMultisampling()
//         .enableAlphaBlending()
//         .colorAttachmentFormat(backend.backbufferImage.format)
//         .depthFormat(backend.depthImage.format)
//         .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
//         .build(backend.device, pass.pipeline->pipelineLayout);

//     // Here we should set up the resources in the rendergraph
//     VkExtent2D swapchainSize { static_cast<uint32_t>(backend.viewport.width), static_cast<uint32_t>(backend.viewport.height) };
//     VkRenderingAttachmentInfo* colorAttachmentInfo = new VkRenderingAttachmentInfo();
//     VkClearValue colorClear = {
//        .color = {
//            .uint32 = {0, 0, 0, 0}
//        }
//     };
//     *colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(backend.backbufferImage.view, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//     VkRenderingAttachmentInfo* depthAttachmentInfo = new VkRenderingAttachmentInfo();
//     *depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(backend.depthImage.view);
//     pass.renderingInfo = vkutil::init::renderingInfo(swapchainSize, colorAttachmentInfo, 1, depthAttachmentInfo);

//     BasePassData* basePassData = new BasePassData();
//     pass.draw = [&, basePassData, culledDraws, shadowPassData](VkCommandBuffer cmd, RenderPass& p) {
//         TracyVkZone(backend.currentFrame().tracyCtx, backend.currentFrame().tracyCmdBuffer, "Base pass");

//         if (!basePassData->initialized) 
//         {
//             basePassData->initialized = true;

//             // Vertex + index buffers
//             const uint32_t vertexBufferSize = scene.vertexData.size() * sizeof(decltype(scene.vertexData)::value_type); 
//             const uint32_t indexBufferSize = scene.indices.size() * sizeof(decltype(scene.indices)::value_type);

//             std::println("Vert count: {}, element size: {}, total size: {}", scene.vertexData.size(), sizeof(decltype(scene.vertexData)::value_type), vertexBufferSize);
//             std::println("Index count: {}, element size: {}, total size: {}", scene.indices.size(), sizeof(decltype(scene.indices)::value_type), indexBufferSize);

//             auto info = vkutil::init::bufferCreateInfo(vertexBufferSize,
//                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
//                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
//                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
//             basePassData->vertexDataBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
//                 VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

//             info = vkutil::init::bufferCreateInfo(indexBufferSize,
//                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
//             basePassData->indexBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_GPU_ONLY,
//                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

//             backend.copyBufferWithStaging((void*)scene.vertexData.data(), vertexBufferSize, basePassData->vertexDataBuffer.buffer);
//             backend.copyBufferWithStaging((void*)scene.indices.data(), indexBufferSize, basePassData->indexBuffer.buffer);

//             // Per model data
//             std::vector<ModelData> modelData;
//             modelData.reserve(scene.meshes.size());
//             for (auto& mesh : scene.meshes)
//             {
//                 ModelData data;
//                 // data.albedoTex = mesh.albedoTexture;
//                 // data.normalTex = mesh.normalTexture;
//                 data.textures = glm::vec4(mesh.albedoTexture, mesh.normalTexture, 0.f, 0.f);
//                 data.model = glm::mat4(1.f);
//                 modelData.push_back(data);
//             }
            
//             info = vkutil::init::bufferCreateInfo(indexBufferSize,
//                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
//             basePassData->perModelDataBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_GPU_ONLY,
//                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

//             backend.copyBufferWithStaging((void*)modelData.data(), modelData.size() * sizeof(ModelData), basePassData->perModelDataBuffer.buffer);

//             // Write bindless textures
//             for (int i = 0; i < scene.images.size(); ++i)
//             {
//                 auto& image = scene.images[i];
//                 int mipCount = floor(log2((double)std::min(image.width, image.height))) + 1;

//                 // TODO: different image formats depending on how many channels
//                 // VkDeviceSize imageSize = image.width * image.height * 4 * 1; // 1 byte per channel
//                 VkDeviceSize imageSize = image.image.size();
//                 VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;

//                 auto info = vkutil::init::bufferCreateInfo(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
//                 // TODO: likely incorrect VMA_ALLOCATION and VK_MEMORY_PROPERTY flags
//                 AllocatedBuffer cpuImageBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_CPU_ONLY,
//                     VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//                 // Copy data to GPU
//                 {
//                     uint8_t* dataOnGpu;
//                     vmaMapMemory(backend.allocator, cpuImageBuffer.allocation, (void**)&dataOnGpu);
//                     memcpy(dataOnGpu, (void*)image.image.data(), imageSize);
//                     vmaUnmapMemory(backend.allocator, cpuImageBuffer.allocation);
//                 }

//                 Texture texture;
//                 texture.mipCount = mipCount;

//                 texture.image.extent.width = image.width;
//                 texture.image.extent.height = image.height;
//                 texture.image.extent.depth = 1;

//                 VkImageCreateInfo imgCreateInfo = vkutil::init::imageCreateInfo(imageFormat,
//                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
//                     texture.image.extent, mipCount);

//                 VmaAllocationCreateInfo imgAllocInfo = {};
//                 imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

//                 vmaCreateImage(backend.allocator, &imgCreateInfo, &imgAllocInfo, &texture.image.image,
//                     &texture.image.allocation, nullptr);

//                 backend.immediateSubmit([&](VkCommandBuffer cmd) {
//                     VkImageMemoryBarrier imageMemoryBarrierForTransfer = vkutil::init::imageMemoryBarrier(VK_IMAGE_LAYOUT_UNDEFINED,
//                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.image.image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, mipCount);

//                     vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
//                         0, nullptr, 1, &imageMemoryBarrierForTransfer);

//                     VkBufferImageCopy copyRegion = {};
//                     copyRegion.bufferOffset = 0;
//                     copyRegion.bufferRowLength = 0;
//                     copyRegion.bufferImageHeight = 0;

//                     copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//                     copyRegion.imageSubresource.mipLevel = 0;
//                     copyRegion.imageSubresource.baseArrayLayer = 0;
//                     copyRegion.imageSubresource.layerCount = 1;
//                     copyRegion.imageExtent = texture.image.extent;

//                     vkCmdCopyBufferToImage(cmd, cpuImageBuffer.buffer, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

//                     // Mip generation
//                     VkImageMemoryBarrier finalFormatTransitionBarrier = vkutil::init::imageMemoryBarrier(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
//                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.image.image, VK_ACCESS_TRANSFER_WRITE_BIT,
//                         VK_ACCESS_SHADER_READ_BIT, 1);
//                     VkImageMemoryBarrier mipIntermediateTransitionBarrier = vkutil::init::imageMemoryBarrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.image.image, VK_ACCESS_TRANSFER_WRITE_BIT,
//                         VK_ACCESS_TRANSFER_READ_BIT, 1);
//                     int32_t mipWidth = image.width;
//                     int32_t mipHeight = image.height;
//                     for (int i = 1; i < mipCount; ++i) 
//                     {
//                         int32_t lastMipWidth = mipWidth;
//                         int32_t lastMipHeight = mipHeight;
//                         mipWidth /= 2;
//                         mipHeight /= 2;

//                         // Transition the last mip to SRC_OPTIMAL
//                         mipIntermediateTransitionBarrier.subresourceRange.baseMipLevel = i - 1;
//                         vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &mipIntermediateTransitionBarrier);

//                         // Blit last mip to downsized current one
//                         VkImageBlit blit = vkutil::init::imageBlit(i - 1, { lastMipWidth, lastMipHeight, 1 }, i, { mipWidth, mipHeight, 1 });
//                         vkCmdBlitImage(cmd, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
//                                 texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//                                 1, &blit, VK_FILTER_LINEAR);
            
//                         // Finally transition last mip to SHADER_READ_ONLY_OPTIMAL
//                         finalFormatTransitionBarrier.subresourceRange.baseMipLevel = i - 1;
//                         vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &finalFormatTransitionBarrier);
//                     }

//                     // Transition the highest mip directly to SHADER_READ_ONLY_OPTIMAL
//                     finalFormatTransitionBarrier.subresourceRange.baseMipLevel = mipCount - 1;
//                     finalFormatTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//                     finalFormatTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//                     vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &finalFormatTransitionBarrier);
//                 });

//                 // backend.deinitQueue.enqueue([=]() {
//                 //     //LOG_CALL(vmaDestroyImage(backend.allocator, texture.image.image, texture.image.allocation));
//                 //     vmaDestroyImage(backend.allocator, texture.image.image, texture.image.allocation);
//                 // });
//                 vmaDestroyBuffer(backend.allocator, cpuImageBuffer.buffer, cpuImageBuffer.allocation);

//                 VkImageViewCreateInfo imageViewInfo = vkutil::init::imageViewCreateInfo(VK_FORMAT_R8G8B8A8_SRGB, texture.image.image, VK_IMAGE_ASPECT_COLOR_BIT, mipCount);
//                 vkCreateImageView(backend.device, &imageViewInfo, nullptr, &texture.view);

//                 // TODO: Updating the bindless texture data should be moved
//                 VkSampler sampler;
//                 VkSamplerCreateInfo samplerInfo = vkutil::init::samplerCreateInfo(
//                     VK_FILTER_LINEAR,
//                     VK_SAMPLER_ADDRESS_MODE_REPEAT,
//                     mipCount
//                 );
//                 vkCreateSampler(backend.device, &samplerInfo, nullptr, &sampler);

//                 VkDescriptorImageInfo descriptorImageInfo = vkutil::init::descriptorImageInfo(
//                     sampler,
//                     texture.view,
//                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  
//                 );
//                 VkWriteDescriptorSet descriptorWrite = vkutil::init::writeDescriptorImage(
//                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//                     backend.bindlessTexDesc,
//                     &descriptorImageInfo,
//                     0
//                 );
//                 descriptorWrite.dstArrayElement = i;

//                 vkUpdateDescriptorSets(backend.device, 1, &descriptorWrite, 0, nullptr);
//             }
//         }

//         // Shadowmap upload
//         // NOTE: this should be done by rendergraph
//         {
//             VkSampler sampler;
//             VkSamplerCreateInfo samplerInfo = vkutil::init::samplerCreateInfo(
//                 VK_FILTER_LINEAR,
//                 VK_SAMPLER_ADDRESS_MODE_REPEAT,
//                 0
//             );
//             vkCreateSampler(backend.device, &samplerInfo, nullptr, &sampler);

//             VkDescriptorImageInfo descriptorImageInfo = vkutil::init::descriptorImageInfo(
//                 sampler,
//                 shadowPassData->shadowMap.view,
//                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  
//             );
//             VkWriteDescriptorSet descriptorWrite = vkutil::init::writeDescriptorImage(
//                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//                 backend.bindlessTexDesc,
//                 &descriptorImageInfo,
//                 0
//             );
//             descriptorWrite.dstArrayElement = scene.images.size();

//             vkUpdateDescriptorSets(backend.device, 1, &descriptorWrite, 0, nullptr);
//         }


//         VkBufferDeviceAddressInfo vertexAddressInfo{ 
//             .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, 
//             .buffer = basePassData->vertexDataBuffer.buffer 
//         };
//         VkBufferDeviceAddressInfo perModelDataAddressInfo{ 
//             .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, 
//             .buffer = basePassData->perModelDataBuffer.buffer 
//         };
//         VkBufferDeviceAddressInfo shadowDataAddressInfo{ 
//             .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, 
//             .buffer = shadowPassData->shadowMapData.buffer
//         };

//         // static bool renderCascades = false;
//         // static bool released = true;
//         // if (glfwGetKey(backend.window, GLFW_KEY_Q) == GLFW_PRESS && released)
//         // {
//         //     released = false;
//         //     renderCascades = !renderCascades;
//         // }
//         // if (glfwGetKey(backend.window, GLFW_KEY_Q) == GLFW_RELEASE)
//         // {
//         //     released = true;
//         // }

//         static bool open = true;
//         static bool renderCascades = false;
//         if (ImGui::Begin("Render debug", &open))
//         {
//             if (ImGui::CollapsingHeader("Base pass"))
//             {
//                 ImGui::Checkbox("Render cascades", &renderCascades);
//             }
//         }
//         ImGui::End();

//         PushConstants pushConstants 
//         {
//             .model = glm::mat4(1.f), //SRT 
//             .color = glm::vec4(renderCascades ? 1.f : 0.f, 0.f, 0.f, 1.f),
//             .vertexBufferAddr = vkGetBufferDeviceAddress(backend.device, &vertexAddressInfo),
//             .perModelDataBufferAddr = vkGetBufferDeviceAddress(backend.device, &perModelDataAddressInfo),
//             .shadowData = vkGetBufferDeviceAddress(backend.device, &shadowDataAddressInfo),
//             .shadowMapIndex = scene.images.size()
//         };
//         vkCmdPushConstants(cmd, p.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pushConstants);
//     	vkCmdBindIndexBuffer(cmd, basePassData->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

//         for (int i = 0; i < 1; i++)
//         {
//             vkCmdDrawIndexedIndirect(cmd, culledDraws.buffer, i * sizeof(VkDrawIndexedIndirectCommand), scene.meshes.size(), sizeof(VkDrawIndexedIndirectCommand));
//         }
//     };

//     graph.renderpasses.push_back(pass);
// }
