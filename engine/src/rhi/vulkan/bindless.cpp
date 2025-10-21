#include "rhi/vulkan/bindless.h"

#include <cmath>
#include <random>

#include "debugUI.h"
#include "imgui_impl_vulkan.h"
#include "renderGraph.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/descriptors.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/utils/texture.h"

BindlessResources::BindlessResources(VulkanBackend& backend) : backend(&backend)
{
    constexpr u32 maxBindlessResourceCount = 10000;
    capacity = maxBindlessResourceCount;
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

auto BindlessResources::addTexture(Texture texture) -> BindlessTexture
{
    // TODO: move this out somewhere else:
    if (texture.name.empty())
    {
        // Some nonsense to generate "unique" name

        static std::random_device dev;
        static std::mt19937 rng(dev());
        std::uniform_int_distribution<int> dist(0, 9);

        texture.name = "generated_";
        texture.name.reserve(10 + 128);
        for (u32 i = 0; i < 128; i++)
        {
            texture.name += std::to_string(dist(rng));
        }
    }

    if (cache.contains(texture.name))
    {
        return cache[texture.name];
    }

    // TODO: Updating the bindless texture data should be moved
    VkDescriptorImageInfo descriptorImageInfo = vkutil::init::descriptorImageInfo(
        texture.sampler, texture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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

    cache[texture.name] = index;

    return index;
}

auto BindlessResources::getTexture(BindlessTexture handle, BindlessTexture defaultTexture) -> const Texture&
{
    if (handle < textures.size() && !freeIndices.contains(handle)) return textures[handle];

    return textures[defaultTexture];
}

auto BindlessResources::removeTexture(BindlessTexture handle) -> void { assert(false); }

template <>
auto addTransition<BindlessTexture>(VulkanBackend& backend, CompiledRenderGraph::Node& node, BindlessTexture* resource,
    Layout oldLayout, Layout newLayout)
    -> void
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

    const Texture& tex = backend.bindlessResources->getTexture(*resource);
    imageBarrier.image = tex.image.image;

    const bool isDepth = tex.image.format == VK_FORMAT_D32_SFLOAT;

    VkImageAspectFlags aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange = vkutil::init::imageSubresourceRange(aspectMask);

    node.imageBarriers.push_back(imageBarrier);
}
