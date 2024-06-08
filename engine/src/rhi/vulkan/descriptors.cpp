#include "rhi/vulkan/descriptors.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

void DescriptorSetLayoutBuilder::addBinding(uint32_t bindingIndex, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = bindingIndex;
    binding.descriptorCount = 1;
    binding.descriptorType = type;

    bindings.push_back(binding);
}

// void DescriptorSetLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
VkDescriptorSetLayout DescriptorSetLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& binding : bindings)
    {
        // TODO: do some verification that this is legal
        binding.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = pNext;

    info.pBindings = bindings.data();
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.flags = flags;

    VkDescriptorSetLayout setLayout;
    // VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &setLayout));
    // TODO: add VK_CHECK
    vkCreateDescriptorSetLayout(device, &info, nullptr, &setLayout);

    return setLayout;
}

void DescriptorAllocator::init(VkDevice device, uint32_t maxSets, std::span<VkDescriptorPoolSize> poolSizes)
{
    VkDescriptorPoolCreateInfo info = {};   
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.pNext = nullptr;

    info.flags = 0;
    // Might be fine
    info.maxSets = maxSets;
    info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();

    // TODO: VK_CHECK
    vkCreateDescriptorPool(device, &info, nullptr, &pool);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = nullptr;

    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet;
    // TODO: VK_CHECK
    vkAllocateDescriptorSets(device, &info, &descriptorSet);

    return descriptorSet;
}
