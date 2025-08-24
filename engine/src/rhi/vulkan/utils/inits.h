#pragma once

#include "engine.h"

#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace vkutil::init
{
VkPipelineShaderStageCreateInfo shaderStageCreateInfo(VkShaderStageFlagBits stageFlags, VkShaderModule shaderModule);
// VkPipelineShaderStageCreateInfo shaderStageCreateInfo(ShaderStage& shaderStage);
VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo();
// VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo(VertexInputDescription& description);
VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo(VkPrimitiveTopology topology);
VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo(VkPolygonMode polygonMode);
VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo();
VkPipelineColorBlendAttachmentState colorBlendAttachmentState();
VkPipelineLayoutCreateInfo layoutCreateInfo();
VkPipelineLayoutCreateInfo layoutCreateInfo(VkDescriptorSetLayout* descriptorSetLayouts,
    u32 descriptorSetLayoutCount, VkPushConstantRange* pushConstantRanges = nullptr,
    u32 pushConsantRangeCount = 0);

VkPushConstantRange pushConstantRange(VkShaderStageFlags shaderStages, u32 size, u32 offset = 0);

VkComputePipelineCreateInfo computePipelineCreateInfo(
    VkPipelineLayout pipelineLayout, VkPipelineShaderStageCreateInfo shaderStageInfo);

VkImageCreateInfo imageCreateInfo(
    VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, u32 mipLevels = 1);
VkImageViewCreateInfo imageViewCreateInfo(
    VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, u32 mipLevels = 1);

VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp compareOp);

VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(
    VkDescriptorType type, VkShaderStageFlags stageFlags, u32 binding);
VkWriteDescriptorSet writeDescriptorBuffer(
    VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, u32 binding);

VkBufferCreateInfo bufferCreateInfo(size_t size, VkBufferUsageFlags flags);

VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags);
VkSubmitInfo submitInfo(VkCommandBuffer* cmd);

VkSamplerCreateInfo samplerCreateInfo(
    VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, f32 maxMip = 0);
VkWriteDescriptorSet writeDescriptorImage(
    VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, u32 binding);

VkPipelineViewportStateCreateInfo pipelineViewportState(
    size_t viewportCount, VkViewport* viewports, size_t scissorCount, VkRect2D* scissor);
VkPipelineColorBlendStateCreateInfo pipelineColorBlendState(bool logicOpEnable, VkLogicOp logicOp,
    size_t attachmentCount, VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates);

VkDescriptorSetAllocateInfo descriptorSetAllocate(
    VkDescriptorPool descriptorPool, size_t descriptorSetCount, VkDescriptorSetLayout* setLayouts);

VkCommandPoolCreateInfo commandPoolCreateInfo(u32 graphicsQueueFamily, VkCommandPoolCreateFlags flags);
VkCommandBufferAllocateInfo commandBufferAllocateInfo(
    u32 commandBufferCount, VkCommandBufferLevel level, VkCommandPool cmdPool);

VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags);
VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags);

VkDescriptorBufferInfo descriptorBufferInfo(VkBuffer buffer, u64 offset, u64 range);
VkDescriptorImageInfo descriptorImageInfo(VkSampler sampler, VkImageView view, VkImageLayout layout);

VkImageSubresourceRange imageSubresourceRange(
    VkImageAspectFlags aspectMask, u32 mipLevels = VK_REMAINING_MIP_LEVELS);
VkImageMemoryBarrier imageMemoryBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image,
    VkPipelineStageFlags srcAccessMask, VkPipelineStageFlags dstAccessMask, u32 mipLevels = 1);
VkImageBlit imageBlit(u32 srcMip, VkOffset3D srcMipSize, u32 dstMip, VkOffset3D dstMipSize);

VkPresentInfoKHR presentInfo(VkSwapchainKHR* swapchains, VkSemaphore* renderSemaphores, u32* imageIndices);

VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);
VkSubmitInfo2 submitInfo2(VkCommandBufferSubmitInfo* cmdSubmitInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo,
    VkSemaphoreSubmitInfo* signalSemaphoreInfo);

VkRenderingAttachmentInfo renderingColorAttachmentInfo(
    VkImageView view, VkClearValue* clear, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
VkRenderingAttachmentInfo renderingDepthAttachmentInfo(
    VkImageView view, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
VkRenderingInfo renderingInfo(VkExtent2D extent, VkRenderingAttachmentInfo* colorAttachments, i32 colorAttachmentCount,
    VkRenderingAttachmentInfo* depthAttachments);
}  // namespace vkutil::init
