#pragma once

#include "engine.h"

#include <vulkan/vulkan.h>

namespace vkutil::init
{
auto shaderStageCreateInfo(VkShaderStageFlagBits stageFlags, VkShaderModule shaderModule)
    -> VkPipelineShaderStageCreateInfo;
auto vertexInputStateCreateInfo() -> VkPipelineVertexInputStateCreateInfo;
auto inputAssemblyCreateInfo(VkPrimitiveTopology topology) -> VkPipelineInputAssemblyStateCreateInfo;
auto rasterizationStateCreateInfo(VkPolygonMode polygonMode) -> VkPipelineRasterizationStateCreateInfo;
auto multisampleStateCreateInfo() -> VkPipelineMultisampleStateCreateInfo;
auto colorBlendAttachmentState() -> VkPipelineColorBlendAttachmentState;
auto layoutCreateInfo() -> VkPipelineLayoutCreateInfo;
auto layoutCreateInfo(VkDescriptorSetLayout* descriptorSetLayouts, u32 descriptorSetLayoutCount,
    VkPushConstantRange* pushConstantRanges = nullptr, u32 pushConsantRangeCount = 0)
    -> VkPipelineLayoutCreateInfo;

auto pushConstantRange(VkShaderStageFlags shaderStages, u32 size, u32 offset = 0) -> VkPushConstantRange;

auto computePipelineCreateInfo(VkPipelineLayout pipelineLayout,
    VkPipelineShaderStageCreateInfo shaderStageInfo) -> VkComputePipelineCreateInfo;

auto imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, u32 mipLevels = 1)
    -> VkImageCreateInfo;
auto imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, u32 mipLevels = 1)
    -> VkImageViewCreateInfo;

auto depthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp compareOp)
    -> VkPipelineDepthStencilStateCreateInfo;

auto descriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, u32 binding)
    -> VkDescriptorSetLayoutBinding;
auto writeDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo,
    u32 binding) -> VkWriteDescriptorSet;

auto bufferCreateInfo(size_t size, VkBufferUsageFlags flags) -> VkBufferCreateInfo;

auto commandBufferBeginInfo(VkCommandBufferUsageFlags flags) -> VkCommandBufferBeginInfo;
auto submitInfo(VkCommandBuffer* cmd) -> VkSubmitInfo;

auto samplerCreateInfo(VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    f32 maxMip = 0) -> VkSamplerCreateInfo;
auto writeDescriptorImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, u32 binding)
    -> VkWriteDescriptorSet;

auto pipelineViewportState(size_t viewportCount, VkViewport* viewports, size_t scissorCount, VkRect2D* scissor)
    -> VkPipelineViewportStateCreateInfo;
auto pipelineColorBlendState(bool logicOpEnable, VkLogicOp logicOp, size_t attachmentCount,
    VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates) -> VkPipelineColorBlendStateCreateInfo;

auto descriptorSetAllocate(VkDescriptorPool descriptorPool, size_t descriptorSetCount,
    VkDescriptorSetLayout* setLayouts) -> VkDescriptorSetAllocateInfo;

auto commandPoolCreateInfo(u32 graphicsQueueFamily, VkCommandPoolCreateFlags flags) -> VkCommandPoolCreateInfo;
auto commandBufferAllocateInfo(u32 commandBufferCount, VkCommandBufferLevel level, VkCommandPool cmdPool)
    -> VkCommandBufferAllocateInfo;

auto fenceCreateInfo(VkFenceCreateFlags flags) -> VkFenceCreateInfo;
auto semaphoreCreateInfo(VkSemaphoreCreateFlags flags) -> VkSemaphoreCreateInfo;

auto descriptorBufferInfo(VkBuffer buffer, u64 offset, u64 range) -> VkDescriptorBufferInfo;
auto descriptorImageInfo(VkSampler sampler, VkImageView view, VkImageLayout layout) -> VkDescriptorImageInfo;

auto imageSubresourceRange(VkImageAspectFlags aspectMask, u32 mipLevels = VK_REMAINING_MIP_LEVELS)
    -> VkImageSubresourceRange;
auto imageMemoryBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image,
    VkPipelineStageFlags srcAccessMask, VkPipelineStageFlags dstAccessMask, u32 mipLevels = 1) -> VkImageMemoryBarrier;
auto imageBlit(u32 srcMip, VkOffset3D srcMipSize, u32 dstMip, VkOffset3D dstMipSize) -> VkImageBlit;

auto presentInfo(VkSwapchainKHR* swapchains, VkSemaphore* renderSemaphores, u32* imageIndices) -> VkPresentInfoKHR;

auto semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) -> VkSemaphoreSubmitInfo;
auto commandBufferSubmitInfo(VkCommandBuffer cmd) -> VkCommandBufferSubmitInfo;
auto submitInfo2(VkCommandBufferSubmitInfo* cmdSubmitInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo,
    VkSemaphoreSubmitInfo* signalSemaphoreInfo) -> VkSubmitInfo2;

auto renderingColorAttachmentInfo(VkImageView view, VkClearValue* clear,
    VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) -> VkRenderingAttachmentInfo;
auto renderingDepthAttachmentInfo(VkImageView view, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) -> VkRenderingAttachmentInfo;
auto renderingInfo(VkExtent3D extent, VkRenderingAttachmentInfo* colorAttachments, i32 colorAttachmentCount,
    VkRenderingAttachmentInfo* depthAttachments) -> VkRenderingInfo;
auto renderingInfo(VkExtent2D extent, VkRenderingAttachmentInfo* colorAttachments, i32 colorAttachmentCount,
    VkRenderingAttachmentInfo* depthAttachments) -> VkRenderingInfo;
}  // namespace vkutil::init
