#include "rhi/vulkan/utils/texture.h"

#include "imgui_impl_vulkan.h"
#include "inits.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/vulkan.h"

#include <cmath>
#include <print>
#include <string>

// auto createTexture(VulkanBackend& backend, std::string name, void* data, u32 size, u32 width, u32 height, bool generateMips) -> Texture
// {
//     const VkDeviceSize imageSize = size;
//     const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
//
//     // NOTE: we can probably refactor this and avoid the vkCmdCopyBufferToImage call altogether by allocating a texture
//     // here.
//     auto info = vkutil::init::bufferCreateInfo(
//         imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
//     AllocatedBuffer cpuImageBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
//         VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//     backend.copyBufferWithStaging(data, imageSize, cpuImageBuffer.buffer);
//
//     Texture texture;
//     texture.name = name;
//
//     u32 mipCount = static_cast<u32>(std::floor(std::log2(std::min(width, height))) + 1);
//     texture.mipCount = generateMips ? mipCount : 1;
//
//     texture.image.extent.width = width;
//     texture.image.extent.height = height;
//     texture.image.extent.depth = 1;
//
//     VkImageCreateInfo imgCreateInfo = vkutil::init::imageCreateInfo(imageFormat,
//         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
//         texture.image.extent, texture.mipCount);
//
//     VmaAllocationCreateInfo imgAllocInfo = {};
//     imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//
//     vmaCreateImage(
//         backend.allocator, &imgCreateInfo, &imgAllocInfo, &texture.image.image, &texture.image.allocation, nullptr);
//
//     VkDebugUtilsObjectNameInfoEXT debugLabelInfo = {
//         .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
//         .pNext = nullptr,
//         .objectType = VK_OBJECT_TYPE_IMAGE,
//         .objectHandle = (u64)texture.image.image,
//         .pObjectName = name.c_str()
//     };
//     backend.vkSetDebugUtilsObjectNameEXT(backend.device, &debugLabelInfo);
//
//     backend.immediateSubmit(
//         [&](VkCommandBuffer cmd)
//         {
//             VkImageMemoryBarrier imageMemoryBarrierForTransfer = vkutil::init::imageMemoryBarrier(
//                 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.image.image, 0,
//                 VK_ACCESS_TRANSFER_WRITE_BIT, texture.mipCount);
//
//             vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
//                 0, nullptr, 1, &imageMemoryBarrierForTransfer);
//
//             VkBufferImageCopy copyRegion = {};
//             copyRegion.bufferOffset = 0;
//             copyRegion.bufferRowLength = 0;
//             copyRegion.bufferImageHeight = 0;
//
//             copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//             copyRegion.imageSubresource.mipLevel = 0;
//             copyRegion.imageSubresource.baseArrayLayer = 0;
//             copyRegion.imageSubresource.layerCount = 1;
//             copyRegion.imageExtent = texture.image.extent;
//
//             vkCmdCopyBufferToImage(
//                 cmd, cpuImageBuffer.buffer, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
//
//             // Mip generation
//             VkImageMemoryBarrier finalFormatTransitionBarrier = vkutil::init::imageMemoryBarrier(
//                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.image.image,
//                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 1);
//             VkImageMemoryBarrier mipIntermediateTransitionBarrier = vkutil::init::imageMemoryBarrier(
//                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.image.image,
//                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, 1);
//             i32 mipWidth = width;
//             i32 mipHeight = height;
//             for (i32 i = 1; i < texture.mipCount; ++i)
//             {
//                 i32 lastMipWidth = mipWidth;
//                 i32 lastMipHeight = mipHeight;
//                 mipWidth /= 2;
//                 mipHeight /= 2;
//
//                 // Transition the last mip to SRC_OPTIMAL
//                 mipIntermediateTransitionBarrier.subresourceRange.baseMipLevel = i - 1;
//                 vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
//                     0, nullptr, 1, &mipIntermediateTransitionBarrier);
//
//                 // Blit last mip to downsized current one
//                 VkImageBlit blit = vkutil::init::imageBlit(
//                     i - 1, {lastMipWidth, lastMipHeight, 1}, i, {mipWidth, mipHeight, 1});
//                 vkCmdBlitImage(cmd, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.image.image,
//                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
//
//                 // Finally transition last mip to SHADER_READ_ONLY_OPTIMAL
//                 finalFormatTransitionBarrier.subresourceRange.baseMipLevel = i - 1;
//                 vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
//                     nullptr, 0, nullptr, 1, &finalFormatTransitionBarrier);
//             }
//
//             // Transition the highest mip directly to SHADER_READ_ONLY_OPTIMAL
//             finalFormatTransitionBarrier.subresourceRange.baseMipLevel = texture.mipCount - 1;
//             finalFormatTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//             finalFormatTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//             vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
//                 nullptr, 0, nullptr, 1, &finalFormatTransitionBarrier);
//         });
//
//     vmaDestroyBuffer(backend.allocator, cpuImageBuffer.buffer, cpuImageBuffer.allocation);
//
//     VkImageViewCreateInfo imageViewInfo = vkutil::init::imageViewCreateInfo(
//         VK_FORMAT_R8G8B8A8_UNORM, texture.image.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.mipCount);
//     vkCreateImageView(backend.device, &imageViewInfo, nullptr, &texture.image.view);
//
//     VkSamplerCreateInfo samplerInfo = vkutil::init::samplerCreateInfo(
//         VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, static_cast<f32>(texture.mipCount));
//     VK_CHECK(vkCreateSampler(backend.device, &samplerInfo, nullptr, &texture.sampler));
//
//     // texture.imguiDescriptorSet = ImGui_ImplVulkan_AddTexture(texture.sampler, texture.image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//
//     return texture;
// }

auto whiteTexture(VulkanBackend& backend, u32 dimension) -> Texture
{
    const u32 textureSize = dimension * dimension * 4;
    std::vector<u8> bytes;
    bytes.reserve(textureSize);
    for (i32 i = 0; i < dimension * dimension; ++i)
    {
        bytes.push_back(255);
        bytes.push_back(255);
        bytes.push_back(255);
        bytes.push_back(255);
    }
    // return createTexture(backend, "builtin_white", bytes.data(), textureSize, dimension, dimension, false);
    auto raw = RawTexture{bytes.data(), textureSize, {dimension, dimension, 1}};
    return backend.createTexture("builtin_white", raw,
        vkutil::init::defaultColorTextureCreateInfo(raw.extent, MipOptions::one().count(), VK_FORMAT_R8G8B8A8_UNORM),
        vkutil::init::defaultTextureAllocationCreateInfo(), MipOptions::one(), VK_IMAGE_ASPECT_COLOR_BIT);
}

auto blackTexture(VulkanBackend& backend, u32 dimension) -> Texture
{
    const u32 textureSize = dimension * dimension * 4;
    std::vector<u8> bytes;
    bytes.reserve(textureSize);
    for (i32 i = 0; i < dimension * dimension; ++i)
    {
        bytes.push_back(0);
        bytes.push_back(0);
        bytes.push_back(0);
        bytes.push_back(255);
    }
    // return createTexture(backend, "builtin_black", bytes.data(), textureSize, dimension, dimension, false);
    auto raw = RawTexture{bytes.data(), textureSize, {dimension, dimension, 1}};
    return backend.createTexture("builtin_black", raw,
        vkutil::init::defaultColorTextureCreateInfo(raw.extent, MipOptions::one().count(), VK_FORMAT_R8G8B8A8_UNORM),
        vkutil::init::defaultTextureAllocationCreateInfo(), MipOptions::one(), VK_IMAGE_ASPECT_COLOR_BIT);
}

auto errorTexture(VulkanBackend& backend, u32 dimension) -> Texture
{
    const u32 textureSize = dimension * dimension * 4;
    std::vector<u8> bytes;
    bytes.reserve(textureSize);
    for (u32 y = 0; y < dimension; ++y)
    {
        for (u32 x = 0; x < dimension; ++x)
        {
            const u32 stridedY = y / 8;
            const u32 stridedX = x / 8;
            const bool magentaArea = (stridedX + stridedY) % 2 != 0;
            const u8 redBlue = magentaArea ? 255 : 0;

            bytes.push_back(redBlue);
            bytes.push_back(0);
            bytes.push_back(redBlue);
            bytes.push_back(255);
        }
    }
    // return createTexture(backend, "builtin_error", bytes.data(), textureSize, dimension, dimension, false);
    auto raw = RawTexture{bytes.data(), textureSize, {dimension, dimension, 1}};
    return backend.createTexture("builtin_error", raw,
        vkutil::init::defaultColorTextureCreateInfo(raw.extent, MipOptions::one().count(), VK_FORMAT_R8G8B8A8_UNORM),
        vkutil::init::defaultTextureAllocationCreateInfo(), MipOptions::one(), VK_IMAGE_ASPECT_COLOR_BIT);
}

// auto Textures::loadRaw(std::string name, RawTexture rawTexture, bool generateMips)
auto Textures::loadRaw(void* data, u32 size, u32 width, u32 height, bool generateMips, bool cache, std::string name)
    // -> CacheResult<Texture&>
    -> std::optional<std::tuple<Texture, std::string>>
{
    if (name.empty())
    {
        static i32 genTextureNameCounter = 0;
        name = std::format("texture_{}", genTextureNameCounter);
        genTextureNameCounter += 1;
        std::println("Naming raw texture: {}", name);
    }

    const auto inCache = textureCache.contains(name);
    if (!inCache)
    {
        // textureCache[name] = createTexture(*backend, name, data, size, width, height, generateMips);
        auto rawTexture = RawTexture {
            .data = data,
            .size = size,
            .extent = { width, height, 1 },
        };
        auto mips = MipOptions::generateAll(rawTexture);
        textureCache[name] = backend->createTexture(name, rawTexture,
            vkutil::init::defaultColorTextureCreateInfo(rawTexture.extent, mips.count(), VK_FORMAT_R8G8B8A8_UNORM),
            vkutil::init::defaultTextureAllocationCreateInfo(),
            mips,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }
    // texture.loadedFromFile = true;

    return std::make_tuple(textureCache[name], name);
}

auto Textures::unload(std::string name) -> void
{
    if (!textureCache.contains(name))
    {
        return;
    }

    unloadRaw(textureCache[name]);
    textureCache.erase(name);
}

auto Textures::unloadRaw(Texture texture) -> void
{
    vmaDestroyImage(backend->allocator, texture.image.image, texture.image.allocation);
    vkDestroyImageView(backend->device, texture.image.view, nullptr);
}
