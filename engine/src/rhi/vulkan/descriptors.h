#pragma once

#include "engine.h"
#include "vulkan/vulkan.h"

#include <span>
#include <vector>


struct DescriptorSetLayoutBuilder
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> bindingFlags;

    void addBinding(
        u32 bindingIndex, VkDescriptorType type, VkDescriptorBindingFlags flags = {}, u32 size = 1);

    VkDescriptorSetLayout build(
        VkDevice device, VkShaderStageFlags shaderStages, VkDescriptorSetLayoutCreateFlags flags = {});
};

struct DescriptorAllocator
{
    VkDescriptorPool pool;

    void init(VkDevice device, u32 maxSets, std::span<VkDescriptorPoolSize> poolSizes,
        VkDescriptorPoolCreateFlags flags = 0);
    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);
};
