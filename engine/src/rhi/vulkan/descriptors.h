#pragma once

#include <span>
#include <vector>

#include "vulkan/vulkan.h"

struct DescriptorSetLayoutBuilder
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> bindingFlags;

    void addBinding(
        uint32_t bindingIndex, VkDescriptorType type, VkDescriptorBindingFlags flags = {}, uint32_t size = 1);

    VkDescriptorSetLayout build(
        VkDevice device, VkShaderStageFlags shaderStages, VkDescriptorSetLayoutCreateFlags flags = {});
};

struct DescriptorAllocator
{
    VkDescriptorPool pool;

    void init(VkDevice device, uint32_t maxSets, std::span<VkDescriptorPoolSize> poolSizes,
        VkDescriptorPoolCreateFlags flags = 0);
    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);
};
