#include "rhi/vulkan/utils/bindless.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/descriptors.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/utils/texture.h"
#include "renderGraph.h"

BindlessResources::BindlessResources(VulkanBackend& backend) : backend(&backend)
{
    constexpr uint32_t maxBindlessResourceCount = 10000;
    {
        VkDescriptorPoolSize poolSizes[] = {
            VkDescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = maxBindlessResourceCount},
        };
        bindlessDescPoolAllocator.init(
            backend.device, maxBindlessResourceCount, poolSizes, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
    }

    constexpr VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                       VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                       VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorSetCount = 1,
        // TODO: This should be an actual array that actually depends on how many descriptors we have
        // For now it's only textures tho.
        .pDescriptorCounts = &maxBindlessResourceCount
    };

    DescriptorSetLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindlessFlags, maxBindlessResourceCount);
    // TODO: Add more bindings for buffers, etc.
    bindlessTexDescLayout = builder.build(
        backend.device, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    bindlessTexDesc = bindlessDescPoolAllocator.allocate(
        backend.device, bindlessTexDescLayout, &variableDescriptorCountAllocInfo);

    // Default textures. No need to deallocate -- we need these to always exist
    addTexture(whiteTexture(backend, 1));
    addTexture(blackTexture(backend, 1));
    addTexture(errorTexture(backend, 64));
}

BindlessTexture BindlessResources::addTexture(Texture texture)
{
    // TODO: Updating the bindless texture data should be moved
    VkSampler sampler;
    VkSamplerCreateInfo samplerInfo = vkutil::init::samplerCreateInfo(
        VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, texture.mipCount);
    vkCreateSampler(backend->device, &samplerInfo, nullptr, &sampler);

    VkDescriptorImageInfo descriptorImageInfo = vkutil::init::descriptorImageInfo(
        sampler, texture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkWriteDescriptorSet descriptorWrite = vkutil::init::writeDescriptorImage(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindlessTexDesc, &descriptorImageInfo, 0);

    BindlessTexture index = lastUsedIndex;
    if (!freeIndices.empty())
    {
        index = *freeIndices.begin();
        freeIndices.erase(index);
    }
    else
    {
        textures.emplace_back();
        lastUsedIndex += 1;
    }
    textures[index] = texture;
    descriptorWrite.dstArrayElement = index;

    vkUpdateDescriptorSets(backend->device, 1, &descriptorWrite, 0, nullptr);

    return index;
}

const Texture& BindlessResources::getTexture(BindlessTexture handle, BindlessTexture defaultTexture)
{
    if (handle < textures.size() && !freeIndices.contains(handle)) return textures[handle];

    return textures[defaultTexture];
}

void BindlessResources::removeTexture(BindlessTexture handle) { assert(false); }

template <>
void addTransition<BindlessTexture>(VulkanBackend& backend, CompiledRenderGraph::Node& node, BindlessTexture* resource, Layout oldLayout, Layout newLayout)
{
    if (oldLayout == newLayout)
    {
        return;
    }

    VkImageMemoryBarrier2 imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier.pNext = nullptr;

    // TODO: we can improve this
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageBarrier.oldLayout = oldLayout;
    imageBarrier.newLayout = newLayout;

    //const bool isDepth = newLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL ||
    //    newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
    //    newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ||
    //    newLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ||
    //    newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL ||
    //    newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ||
    //    newLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;

    //const Texture& tex = graph.backend.bindlessResources->getTexture(*resource);
    const Texture& tex = backend.bindlessResources->getTexture(*resource);
    imageBarrier.image = tex.image.image;

    const bool isDepth = tex.image.format == VK_FORMAT_D32_SFLOAT;

    VkImageAspectFlags aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange = vkutil::init::imageSubresourceRange(aspectMask);

    node.imageBarriers.push_back(imageBarrier);
}
