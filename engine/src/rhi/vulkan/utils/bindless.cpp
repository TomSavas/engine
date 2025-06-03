#include "rhi/vulkan/utils/bindless.h"

#include "rhi/vulkan/descriptors.h"

#include <vulkan/vulkan_core.h>

BindlessResources::BindlessResources(RHIBackend& backend) : backend(backend)
{

    const VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | 
                                             VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | 
                                             VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorSetCount = 1,
        // TODO: This should be an actual array that actually depends on how many descriptors we have
        // For now it's only textures tho.
        .pDescriptorCounts = &maxBindlessResourceCount
    };
    
    DescriptorSetLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindlessFlags, maxBindlessResourceCount);
    // Add more bindings for buffers, etc.
    bindlessTexDescLayout = builder.build(device, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    bindlessTexDesc = bindlessDescPoolAllocator.allocate(device, bindlessTexDescLayout, &variableDescriptorCountAllocInfo);
}

BindlessResources::Handle BindlessResources::addTexture(Texture texture)
{
    // TODO: Updating the bindless texture data should be moved
    VkSampler sampler;
    VkSamplerCreateInfo samplerInfo = vkutil::init::samplerCreateInfo(
        VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        texture.mipCount
    );
    vkCreateSampler(backend.device, &samplerInfo, nullptr, &sampler);

    VkDescriptorImageInfo descriptorImageInfo = vkutil::init::descriptorImageInfo(
        sampler,
        texture.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  
    );
    VkWriteDescriptorSet descriptorWrite = vkutil::init::writeDescriptorImage(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        backend.bindlessTexDesc,
        &descriptorImageInfo,
        0
    );

	Handle index = lastUsedIndex;
	if (freeIndices.empty())
	{
		index = freeIndices.pop_back();
	}
	else
	{
		lastUsedIndex += 1;
	}
    descriptorWrite.dstArrayElement = index;

    vkUpdateDescriptorSets(backend.device, 1, &descriptorWrite, 0, nullptr);

    return index;
}

void BindlessResources::removeTexture(Handle handle)
{
	//LOL NE
}
