#include "rhi/vulkan/descriptors.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>

#include "rhi/vulkan/vulkan.h"

auto DescriptorSetLayoutBuilder::addBinding(u32 bindingIndex, VkDescriptorType type, VkDescriptorBindingFlags flags,
    u32 count) -> void
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = bindingIndex;
    binding.descriptorCount = count;
    binding.descriptorType = type;

    bindings.push_back(binding);
    bindingFlags.push_back(flags);
}

auto DescriptorSetLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages,
    VkDescriptorSetLayoutCreateFlags flags) -> VkDescriptorSetLayout
{
    for (auto& binding : bindings)
    {
        // TODO: do some verification that this is legal
        binding.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo = {};
    bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsCreateInfo.pNext = nullptr;
    bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();
    bindingFlagsCreateInfo.bindingCount = static_cast<u32>(bindingFlags.size());

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = &bindingFlagsCreateInfo;

    info.pBindings = bindings.data();
    info.bindingCount = static_cast<u32>(bindings.size());
    info.flags = flags;

    VkDescriptorSetLayout setLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &setLayout));

    return setLayout;
}

auto DescriptorAllocator::init(VkDevice device, u32 maxSets, std::span<VkDescriptorPoolSize> poolSizes,
    VkDescriptorPoolCreateFlags flags) -> void
{
    VkDescriptorPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.pNext = nullptr;

    maxSets = std::ranges::fold_left(
        poolSizes, 0, [](u32 total, VkDescriptorPoolSize size) { return size.descriptorCount + total; });

    info.flags = flags;
    info.maxSets = maxSets;
    info.poolSizeCount = static_cast<u32>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();

    VK_CHECK(vkCreateDescriptorPool(device, &info, nullptr, &pool));
}

auto DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext) -> VkDescriptorSet
{
    VkDescriptorSetAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = pNext;

    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet;
    VK_CHECK(vkAllocateDescriptorSets(device, &info, &descriptorSet));

    return descriptorSet;
}
