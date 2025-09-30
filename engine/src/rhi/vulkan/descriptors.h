#pragma once

#include "engine.h"
#include "vulkan/vulkan.h"

#include <span>
#include <vector>

struct DescriptorSetLayoutBuilder
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> bindingFlags;

    auto addBinding(u32 bindingIndex, VkDescriptorType type, VkDescriptorBindingFlags flags = {}, u32 size = 1) -> void;
    auto build(VkDevice device, VkShaderStageFlags shaderStages, VkDescriptorSetLayoutCreateFlags flags = {})
        -> VkDescriptorSetLayout;
};

struct DescriptorAllocator
{
    VkDescriptorPool pool;

    auto init(VkDevice device, u32 maxSets, std::span<VkDescriptorPoolSize> poolSizes,
        VkDescriptorPoolCreateFlags flags = 0) -> void;
    auto allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr) -> VkDescriptorSet;
};
