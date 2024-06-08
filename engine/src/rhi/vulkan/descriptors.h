#pragma once

#include <vector>
#include <span>

#include "vulkan/vulkan.h"

struct DescriptorSetLayoutBuilder 
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
 
    void addBinding(uint32_t bindingIndex, VkDescriptorType type);
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = {});
};

struct DescriptorAllocator
{
    VkDescriptorPool pool;

    struct PoolSizeRatio 
    {
        VkDescriptorType type;
        float ratio;
    };

    void init(VkDevice device, uint32_t maxSets, std::span<VkDescriptorPoolSize> poolSizes);
    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};
