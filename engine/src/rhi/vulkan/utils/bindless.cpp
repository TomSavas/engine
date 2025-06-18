#include "rhi/vulkan/utils/bindless.h"
#include "rhi/vulkan/utils/inits.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/descriptors.h"

BindlessResources::BindlessResources(VulkanBackend& backend) : backend(&backend)
{
    const uint32_t maxBindlessResourceCount = 10000;
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
    bindlessTexDescLayout = builder.build(backend.device, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    // FIXME: pool allocator should not be in the backend
    bindlessTexDesc = backend.bindlessDescPoolAllocator.allocate(backend.device, bindlessTexDescLayout, &variableDescriptorCountAllocInfo);
}

BindlessTexture BindlessResources::addTexture(Texture texture)
{
    // TODO: Updating the bindless texture data should be moved
    VkSampler sampler;
    VkSamplerCreateInfo samplerInfo = vkutil::init::samplerCreateInfo(
        VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        texture.mipCount
    );
    vkCreateSampler(backend->device, &samplerInfo, nullptr, &sampler);

    VkDescriptorImageInfo descriptorImageInfo = vkutil::init::descriptorImageInfo(
        sampler,
        texture.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  
    );
    VkWriteDescriptorSet descriptorWrite = vkutil::init::writeDescriptorImage(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        bindlessTexDesc,
        &descriptorImageInfo,
        0
    );

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

Texture BindlessResources::getTexture(BindlessTexture handle, BindlessTexture defaultTexture)
{
    if (handle < textures.size() && !freeIndices.contains(handle))
        return textures[handle];

    return textures[defaultTexture];
}


void BindlessResources::removeTexture(BindlessTexture handle)
{
    assert(false);
}
