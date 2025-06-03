#pragma once

#include <stdint.h>

class RHIBackend;

struct BindlessResources
{
	using Handle = uint32_t;

	RHIBackend& backend;

    VkDescriptorSet bindlessTexDesc;
    VkDescriptorSetLayout bindlessTexDescLayout;

    // CPU mirror of what data is in the GPU buffer
    std::vector<Texture> textures;
    uint32_t lastUsedIndex;
    int capacity;
    std::vector<Handle> freeIndices; // All free indices that occur before lastUsedIndex
	
	explicit BindlessResources(RHIBackend& backend);

	Handle addTexture(Texture texture);
	void removeTexture(Handle handle);
};
