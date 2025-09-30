#include "rhi/vulkan/utils/inits.h"

#include <vulkan/vulkan_core.h>

namespace vkutil::init
{
auto shaderStageCreateInfo(VkShaderStageFlagBits stageFlags, VkShaderModule shaderModule)
    -> VkPipelineShaderStageCreateInfo
{
    VkPipelineShaderStageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.pNext = nullptr;

    info.stage = stageFlags;
    info.module = shaderModule;
    info.pName = "main";

    return info;
}

auto vertexInputStateCreateInfo() -> VkPipelineVertexInputStateCreateInfo
{
    VkPipelineVertexInputStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.vertexBindingDescriptionCount = 0;
    info.vertexAttributeDescriptionCount = 0;

    return info;
}

auto inputAssemblyCreateInfo(VkPrimitiveTopology topology) -> VkPipelineInputAssemblyStateCreateInfo
{
    VkPipelineInputAssemblyStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.topology = topology;
    // we are not going to use primitive restart on the entire tutorial so leave it on false
    info.primitiveRestartEnable = VK_FALSE;

    return info;
}

auto rasterizationStateCreateInfo(VkPolygonMode polygonMode) -> VkPipelineRasterizationStateCreateInfo
{
    VkPipelineRasterizationStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.depthClampEnable = VK_FALSE;
    // discards all primitives before the rasterization stage if enabled which we don't want
    info.rasterizerDiscardEnable = VK_FALSE;

    info.polygonMode = polygonMode;
    info.lineWidth = 1.0f;
    // no backface cull
    info.cullMode = VK_CULL_MODE_NONE;
    info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    // no depth bias
    info.depthBiasEnable = VK_FALSE;
    info.depthBiasConstantFactor = 0.0f;
    info.depthBiasClamp = 0.0f;
    info.depthBiasSlopeFactor = 0.0f;

    return info;
}

auto multisampleStateCreateInfo() -> VkPipelineMultisampleStateCreateInfo
{
    VkPipelineMultisampleStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.sampleShadingEnable = VK_FALSE;
    // multisampling defaulted to no multisampling (1 sample per pixel)
    info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    info.minSampleShading = 1.0f;
    info.pSampleMask = nullptr;
    info.alphaToCoverageEnable = VK_FALSE;
    info.alphaToOneEnable = VK_FALSE;

    return info;
}

auto colorBlendAttachmentState() -> VkPipelineColorBlendAttachmentState
{
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};

    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    return colorBlendAttachment;
}

auto layoutCreateInfo() -> VkPipelineLayoutCreateInfo
{
    VkPipelineLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = nullptr;

    info.flags = 0;
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;

    return info;
}

auto layoutCreateInfo(VkDescriptorSetLayout* descriptorSetLayouts, u32 descriptorSetLayoutCount,
    VkPushConstantRange* pushConstantRanges, u32 pushConsantRangeCount) -> VkPipelineLayoutCreateInfo
{
    VkPipelineLayoutCreateInfo info = layoutCreateInfo();

    info.setLayoutCount = descriptorSetLayoutCount;
    info.pSetLayouts = descriptorSetLayouts;

    info.pushConstantRangeCount = pushConsantRangeCount;
    info.pPushConstantRanges = pushConstantRanges;

    return info;
}

auto pushConstantRange(VkShaderStageFlags shaderStages, u32 size, u32 offset) -> VkPushConstantRange
{
    VkPushConstantRange range = {};

    range.stageFlags = shaderStages;
    range.size = size;
    range.offset = offset;

    return range;
}

auto computePipelineCreateInfo(VkPipelineLayout pipelineLayout, VkPipelineShaderStageCreateInfo shaderStageInfo)
    -> VkComputePipelineCreateInfo
{
    VkComputePipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.pNext = nullptr;

    info.layout = pipelineLayout;
    info.stage = shaderStageInfo;

    return info;
}

auto imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, u32 mipLevels)
    -> VkImageCreateInfo
{
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;

    info.imageType = VK_IMAGE_TYPE_2D;

    info.format = format;
    info.extent = extent;

    info.mipLevels = mipLevels;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usageFlags;

    return info;
}

auto imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, u32 mipLevels)
    -> VkImageViewCreateInfo
{
    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = nullptr;

    info.viewType = VK_IMAGE_VIEW_TYPE_2D;

    info.image = image;
    info.format = format;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = mipLevels;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.aspectMask = aspectFlags;

    return info;
}

auto depthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp compareOp)
    -> VkPipelineDepthStencilStateCreateInfo
{
    VkPipelineDepthStencilStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.depthTestEnable = depthTest ? VK_TRUE : VK_FALSE;
    info.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
    info.depthCompareOp = depthTest ? compareOp : VK_COMPARE_OP_ALWAYS;
    info.depthBoundsTestEnable = VK_FALSE;
    info.minDepthBounds = 0.f;
    info.maxDepthBounds = 1.f;
    info.stencilTestEnable = VK_FALSE;

    return info;
}

auto descriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, u32 binding)
    -> VkDescriptorSetLayoutBinding
{
    VkDescriptorSetLayoutBinding setLayoutBinding = {};
    setLayoutBinding.binding = binding;
    setLayoutBinding.descriptorCount = 1;
    setLayoutBinding.descriptorType = type;
    setLayoutBinding.pImmutableSamplers = nullptr;
    setLayoutBinding.stageFlags = stageFlags;

    return setLayoutBinding;
}

auto writeDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo,
    u32 binding) -> VkWriteDescriptorSet
{
    VkWriteDescriptorSet writeSet = {};
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSet.pNext = nullptr;

    writeSet.dstBinding = binding;
    writeSet.dstSet = dstSet;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType = type;
    writeSet.pBufferInfo = bufferInfo;

    return writeSet;
}

auto bufferCreateInfo(size_t size, VkBufferUsageFlags usage) -> VkBufferCreateInfo
{
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = nullptr;
    info.size = size;
    info.usage = usage;

    return info;
}

auto commandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0) -> VkCommandBufferBeginInfo
{
    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.pNext = nullptr;

    cmdBeginInfo.pInheritanceInfo = nullptr;
    cmdBeginInfo.flags = flags;

    return cmdBeginInfo;
}

auto submitInfo(VkCommandBuffer* cmd) -> VkSubmitInfo
{
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;

    submitInfo.pWaitDstStageMask = nullptr;

    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;

    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = cmd;

    return submitInfo;
}

auto samplerCreateInfo(VkFilter filters, VkSamplerAddressMode samplerAddressMode, f32 maxMip) -> VkSamplerCreateInfo
{
    VkSamplerCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext = nullptr;

    info.magFilter = filters;
    info.minFilter = filters;
    info.addressModeU = samplerAddressMode;
    info.addressModeV = samplerAddressMode;
    info.addressModeW = samplerAddressMode;

    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.minLod = 0.0;
    info.maxLod = maxMip;
    info.mipLodBias = 0.0;

    return info;
}

auto writeDescriptorImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, u32 binding)
    -> VkWriteDescriptorSet
{
    VkWriteDescriptorSet writeSet = {};
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSet.pNext = nullptr;

    writeSet.dstBinding = binding;
    writeSet.dstSet = dstSet;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType = type;
    writeSet.pImageInfo = imageInfo;

    return writeSet;
}

auto pipelineViewportState(size_t viewportCount, VkViewport* viewports, size_t scissorCount, VkRect2D* scissor)
    -> VkPipelineViewportStateCreateInfo
{
    VkPipelineViewportStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.viewportCount = viewportCount;
    info.pViewports = viewports;
    info.scissorCount = scissorCount;
    info.pScissors = scissor;

    return info;
}

auto pipelineColorBlendState(bool logicOpEnable, VkLogicOp logicOp, size_t attachmentCount,
    VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates) -> VkPipelineColorBlendStateCreateInfo
{
    VkPipelineColorBlendStateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.logicOpEnable = logicOpEnable ? VK_TRUE : VK_FALSE;
    info.logicOp = logicOp;
    info.attachmentCount = attachmentCount;
    info.pAttachments = colorBlendAttachmentStates;

    return info;
}

auto descriptorSetAllocate(VkDescriptorPool descriptorPool, size_t descriptorSetCount,
    VkDescriptorSetLayout* setLayouts) -> VkDescriptorSetAllocateInfo
{
    VkDescriptorSetAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = nullptr;

    info.descriptorPool = descriptorPool;
    info.descriptorSetCount = descriptorSetCount;
    info.pSetLayouts = setLayouts;

    return info;
}

auto commandPoolCreateInfo(u32 graphicsQueueFamily, VkCommandPoolCreateFlags flags) -> VkCommandPoolCreateInfo
{
    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = nullptr;

    info.queueFamilyIndex = graphicsQueueFamily;
    info.flags = flags;

    return info;
}

auto commandBufferAllocateInfo(u32 commandBufferCount, VkCommandBufferLevel level, VkCommandPool cmdPool)
    -> VkCommandBufferAllocateInfo
{
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = nullptr;

    info.commandPool = cmdPool;
    info.level = level;
    info.commandBufferCount = commandBufferCount;

    return info;
}

auto fenceCreateInfo(VkFenceCreateFlags flags) -> VkFenceCreateInfo
{
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = nullptr;

    info.flags = flags;

    return info;
}

auto semaphoreCreateInfo(VkSemaphoreCreateFlags flags) -> VkSemaphoreCreateInfo
{
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = nullptr;

    info.flags = flags;

    return info;
}

auto descriptorBufferInfo(VkBuffer buffer, u64 offset, u64 range) -> VkDescriptorBufferInfo
{
    VkDescriptorBufferInfo info = {};
    info.buffer = buffer;
    info.offset = offset;
    info.range = range;

    return info;
}

auto descriptorImageInfo(VkSampler sampler, VkImageView view, VkImageLayout layout) -> VkDescriptorImageInfo
{
    VkDescriptorImageInfo info = {};
    info.sampler = sampler;
    info.imageView = view;
    info.imageLayout = layout;

    return info;
}

auto imageSubresourceRange(VkImageAspectFlags aspectMask, u32 mipLevels) -> VkImageSubresourceRange
{
    VkImageSubresourceRange range = {};
    range.aspectMask = aspectMask;
    range.baseMipLevel = 0;
    range.levelCount = mipLevels;
    range.baseArrayLayer = 0;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return range;
}

auto imageMemoryBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image,
    VkPipelineStageFlags srcAccessMask, VkPipelineStageFlags dstAccessMask, u32 mipLevels) -> VkImageMemoryBarrier
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = nullptr;

    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange = imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);

    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    return barrier;
}

auto imageBlit(u32 srcMip, VkOffset3D srcMipSize, u32 dstMip, VkOffset3D dstMipSize) -> VkImageBlit
{
    VkImageBlit blit = {};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = srcMipSize;
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = srcMip;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = dstMipSize;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = dstMip;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    return blit;
}

auto presentInfo(VkSwapchainKHR* swapchains, VkSemaphore* renderSemaphores, u32* imageIndices) -> VkPresentInfoKHR
{
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pNext = nullptr;

    info.pSwapchains = swapchains;
    info.swapchainCount = swapchains == nullptr ? 0 : 1;

    info.pWaitSemaphores = renderSemaphores;
    info.waitSemaphoreCount = 1;

    info.pImageIndices = imageIndices;

    return info;
}

auto semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) -> VkSemaphoreSubmitInfo
{
    VkSemaphoreSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    info.pNext = nullptr;

    info.semaphore = semaphore;
    info.stageMask = stageMask;
    info.deviceIndex = 0;
    info.value = 1;

    return info;
}

auto commandBufferSubmitInfo(VkCommandBuffer cmd) -> VkCommandBufferSubmitInfo
{
    VkCommandBufferSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    info.pNext = nullptr;

    info.commandBuffer = cmd;
    info.deviceMask = 0;

    return info;
}

auto submitInfo2(VkCommandBufferSubmitInfo* cmdSubmitInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo,
    VkSemaphoreSubmitInfo* signalSemaphoreInfo) -> VkSubmitInfo2
{
    VkSubmitInfo2 info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    info.pNext = nullptr;

    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmdSubmitInfo;

    return info;
}

auto renderingColorAttachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout)
    -> VkRenderingAttachmentInfo
{
    VkRenderingAttachmentInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    info.pNext = nullptr;

    info.imageView = view;
    info.imageLayout = layout;
    info.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (clear != nullptr)
    {
        info.clearValue = *clear;
    }

    return info;
}

auto renderingDepthAttachmentInfo(VkImageView view, VkAttachmentLoadOp loadOp, VkImageLayout layout)
    -> VkRenderingAttachmentInfo
{
    VkRenderingAttachmentInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    info.pNext = nullptr;

    info.imageView = view;
    info.imageLayout = layout;
    info.loadOp = loadOp;
    info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    info.clearValue.depthStencil.depth = 1.f;

    return info;
}

auto renderingInfo(VkExtent3D extent, VkRenderingAttachmentInfo* colorAttachments, i32 colorAttachmentCount,
    VkRenderingAttachmentInfo* depthAttachments) -> VkRenderingInfo
{
    return renderingInfo(VkExtent2D{extent.width, extent.height}, colorAttachments, colorAttachmentCount,
        depthAttachments);
}

auto renderingInfo(VkExtent2D extent, VkRenderingAttachmentInfo* colorAttachments, i32 colorAttachmentCount,
    VkRenderingAttachmentInfo* depthAttachments) -> VkRenderingInfo
{
    VkRenderingInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.pNext = nullptr;

    info.renderArea = VkRect2D{VkOffset2D{0, 0}, extent};
    info.layerCount = 1;
    info.colorAttachmentCount = colorAttachmentCount;
    info.pColorAttachments = colorAttachments;
    info.pDepthAttachment = depthAttachments;
    info.pStencilAttachment = nullptr;

    return info;
}
}  // namespace vkutil::init
