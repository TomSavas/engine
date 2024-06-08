#pragma once

#include <stdint.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace vkutil::init
{
VkPipelineShaderStageCreateInfo shaderStageCreateInfo(VkShaderStageFlagBits stageFlags, VkShaderModule shaderModule);
//VkPipelineShaderStageCreateInfo shaderStageCreateInfo(ShaderStage& shaderStage);
VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo();
//VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo(VertexInputDescription& description);
VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo(VkPrimitiveTopology topology);
VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo(VkPolygonMode polygonMode);
VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo();
VkPipelineColorBlendAttachmentState colorBlendAttachmentState();
VkPipelineLayoutCreateInfo layoutCreateInfo();
VkPipelineLayoutCreateInfo layoutCreateInfo(VkDescriptorSetLayout* descriptorSetLayouts, uint32_t descriptorSetLayoutCount);

VkComputePipelineCreateInfo computePipelineCreateInfo(VkPipelineLayout pipelineLayout, VkPipelineShaderStageCreateInfo shaderStageInfo);

VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, uint32_t mipLevels = 1);
VkImageViewCreateInfo imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, uint32_t mipLevels = 1);

VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp compareOp);

VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
VkWriteDescriptorSet writeDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);

VkBufferCreateInfo bufferCreateInfo(size_t size, VkBufferUsageFlags flags);

VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags);
VkSubmitInfo submitInfo(VkCommandBuffer* cmd);

VkSamplerCreateInfo samplerCreateInfo(VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, float maxMip = 0);
VkWriteDescriptorSet writeDescriptorImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding);

VkPipelineViewportStateCreateInfo pipelineViewportState(size_t viewportCount, VkViewport* viewports, size_t scissorCount, VkRect2D* scissor);
VkPipelineColorBlendStateCreateInfo pipelineColorBlendState(bool logicOpEnable, VkLogicOp logicOp, size_t attachmentCount, VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates);

VkDescriptorSetAllocateInfo descriptorSetAllocate(VkDescriptorPool descriptorPool, size_t descriptorSetCount, VkDescriptorSetLayout* setLayouts);

VkCommandPoolCreateInfo commandPoolCreateInfo(uint32_t graphicsQueueFamily, VkCommandPoolCreateFlags flags);
VkCommandBufferAllocateInfo commandBufferAllocateInfo(uint32_t commandBufferCount, VkCommandBufferLevel level, VkCommandPool cmdPool);

VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags);
VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags);

VkDescriptorBufferInfo descriptorBufferInfo(VkBuffer buffer, uint64_t offse, uint64_t range);
VkDescriptorImageInfo descriptorImageInfo(VkSampler sampler, VkImageView view, VkImageLayout layout);

VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask, uint32_t mipLevels = VK_REMAINING_MIP_LEVELS);
VkImageMemoryBarrier imageMemoryBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image, VkPipelineStageFlags srcAccessMask, VkPipelineStageFlags dstAccessMask, uint32_t mipLevels = 1);
VkImageBlit imageBlit(uint32_t srcMip, VkOffset3D srcMipSize, uint32_t dstMip, VkOffset3D dstMipSize);

VkPresentInfoKHR presentInfo(VkSwapchainKHR* swapchains, VkSemaphore* renderSemaphores, uint32_t* imageIndices);

VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);
VkSubmitInfo2 submitInfo2(VkCommandBufferSubmitInfo* cmdSubmitInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo, VkSemaphoreSubmitInfo* signalSemaphoreInfo);

VkRenderingAttachmentInfo renderingColorAttachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
VkRenderingInfo renderingInfo(VkExtent2D extent, VkRenderingAttachmentInfo* colorAttachments, VkRenderingAttachmentInfo* depthAttachments);
}
