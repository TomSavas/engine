#pragma once

#include <vulkan/vulkan.h>

#include <unordered_set>
#include <vector>

#include "engine.h"
#include "rhi/vulkan/descriptors.h"
#include "rhi/vulkan/utils/texture.h"

class VulkanBackend;

using BindlessTexture = u32;
struct BindlessResources
{
    static constexpr BindlessTexture kWhite = 0;
    static constexpr BindlessTexture kBlack = 1;
    static constexpr BindlessTexture kError = 2;

    VulkanBackend* backend;

    DescriptorAllocator bindlessDescPoolAllocator;

    VkDescriptorSet bindlessTexDesc;
    VkDescriptorSetLayout bindlessTexDescLayout;

    // CPU mirror of what data is in the GPU buffer
    std::vector<Texture> textures;
    BindlessTexture lastUsedIndex;
    i32 capacity;
    std::unordered_set<BindlessTexture> freeIndices;  // All free indices that occur before lastUsedIndex

    explicit BindlessResources(VulkanBackend& backend);

    auto addTexture(Texture texture) -> BindlessTexture;
    auto getTexture(BindlessTexture handle, BindlessTexture defaultTexture = kError) -> const Texture&;
    auto removeTexture(BindlessTexture handle) -> void;
};
