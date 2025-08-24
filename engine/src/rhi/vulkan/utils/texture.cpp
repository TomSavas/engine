#include "rhi/vulkan/utils/texture.h"

#include <print>
#include <string>

#include "inits.h"
#include "rhi/vulkan/backend.h"

// std::optional<Texture> Textures::load(std::string path, bool generateMips, bool cache)
// {
//     auto cachedTexture = textureCache.find(path);
//     if (cachedTexture != textureCache.end())
//     {
//         return cachedTexture->second;
//     }
//     else
//     {
//         std::println("Loading new texture {}", path);
//     }

//     i32 width;
//     i32 height;
//     i32 channels;
//     stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

//     if (!pixels)
//     {
// 		std::println("Failed to load texture file {}", path);
//         return std::optional{};
//     }

// 	return loadRaw(pixels, width * height * channels * 1, width, height, generateMips, cache, path);
// }

Texture createTexture(
    VulkanBackend& backend, void* data, u32 size, u32 width, u32 height, bool generateMips)
{
    const VkDeviceSize imageSize = size;
    //const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
    const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

    // NOTE: we can probably refactor this and avoid the vkCmdCopyBufferToImage call altogether by allocating a texture
    // here.
    auto info = vkutil::init::bufferCreateInfo(
        imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    AllocatedBuffer cpuImageBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    backend.copyBufferWithStaging(data, imageSize, cpuImageBuffer.buffer);

    Texture texture;

    u32 mipCount = static_cast<u32>(floor(log2(std::min(width, height))) + 1);
    texture.mipCount = generateMips ? mipCount : 1;

    texture.image.extent.width = width;
    texture.image.extent.height = height;
    texture.image.extent.depth = 1;

    VkImageCreateInfo imgCreateInfo = vkutil::init::imageCreateInfo(imageFormat,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        texture.image.extent, texture.mipCount);

    VmaAllocationCreateInfo imgAllocInfo = {};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(
        backend.allocator, &imgCreateInfo, &imgAllocInfo, &texture.image.image, &texture.image.allocation, nullptr);

    backend.immediateSubmit(
        [&](VkCommandBuffer cmd)
        {
            VkImageMemoryBarrier imageMemoryBarrierForTransfer = vkutil::init::imageMemoryBarrier(
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.image.image, 0,
                VK_ACCESS_TRANSFER_WRITE_BIT, texture.mipCount);

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                0, nullptr, 1, &imageMemoryBarrierForTransfer);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = texture.image.extent;

            vkCmdCopyBufferToImage(
                cmd, cpuImageBuffer.buffer, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            // Mip generation
            VkImageMemoryBarrier finalFormatTransitionBarrier = vkutil::init::imageMemoryBarrier(
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.image.image,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 1);
            VkImageMemoryBarrier mipIntermediateTransitionBarrier = vkutil::init::imageMemoryBarrier(
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.image.image,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, 1);
            i32 mipWidth = width;
            i32 mipHeight = height;
            for (i32 i = 1; i < texture.mipCount; ++i)
            {
                i32 lastMipWidth = mipWidth;
                i32 lastMipHeight = mipHeight;
                mipWidth /= 2;
                mipHeight /= 2;

                // Transition the last mip to SRC_OPTIMAL
                mipIntermediateTransitionBarrier.subresourceRange.baseMipLevel = i - 1;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                    0, nullptr, 1, &mipIntermediateTransitionBarrier);

                // Blit last mip to downsized current one
                VkImageBlit blit = vkutil::init::imageBlit(
                    i - 1, {lastMipWidth, lastMipHeight, 1}, i, {mipWidth, mipHeight, 1});
                vkCmdBlitImage(cmd, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.image.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

                // Finally transition last mip to SHADER_READ_ONLY_OPTIMAL
                finalFormatTransitionBarrier.subresourceRange.baseMipLevel = i - 1;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                    nullptr, 0, nullptr, 1, &finalFormatTransitionBarrier);
            }

            // Transition the highest mip directly to SHADER_READ_ONLY_OPTIMAL
            finalFormatTransitionBarrier.subresourceRange.baseMipLevel = texture.mipCount - 1;
            finalFormatTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            finalFormatTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                nullptr, 0, nullptr, 1, &finalFormatTransitionBarrier);
        });

    vmaDestroyBuffer(backend.allocator, cpuImageBuffer.buffer, cpuImageBuffer.allocation);

    VkImageViewCreateInfo imageViewInfo = vkutil::init::imageViewCreateInfo(
        VK_FORMAT_R8G8B8A8_UNORM, texture.image.image, VK_IMAGE_ASPECT_COLOR_BIT, texture.mipCount);
    vkCreateImageView(backend.device, &imageViewInfo, nullptr, &texture.view);

    return texture;
}

Texture whiteTexture(VulkanBackend& backend, u32 dimension)
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
    return createTexture(backend, bytes.data(), textureSize, dimension, dimension, false);
}

Texture blackTexture(VulkanBackend& backend, u32 dimension)
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
    return createTexture(backend, bytes.data(), textureSize, dimension, dimension, false);
}

Texture errorTexture(VulkanBackend& backend, u32 dimension)
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
    return createTexture(backend, bytes.data(), textureSize, dimension, dimension, false);
}

std::optional<std::tuple<Texture, std::string>> Textures::loadRaw(
    void* data, u32 size, u32 width, u32 height, bool generateMips, bool cache, std::string name)
{
    if (name.empty())
    {
        static i32 genTextureNameCounter = 0;
        name = std::format("texture_{}", genTextureNameCounter);
        genTextureNameCounter += 1;
        std::println("Naming raw texture: {}", name);
    }

    //std::print("Loading raw texture {}... ", name);

    if (const auto tex = textureCache.find(name); tex != textureCache.end())
    {
        //std::println("Found in cache!");
        return std::make_tuple(tex->second, name);
    }

    //std::println("Not in cache, loading...");
    Texture texture = createTexture(*backend, data, size, width, height, generateMips);

    if (cache)
    {
        textureCache[name] = texture;
    }

    return std::make_tuple(texture, name);
}

void Textures::unload(std::string name)
{
    if (!textureCache.contains(name))
    {
        return;
    }

    unloadRaw(textureCache[name]);
    textureCache.erase(name);
}

void Textures::unloadRaw(Texture texture)
{
    vmaDestroyImage(backend->allocator, texture.image.image, texture.image.allocation);
    vkDestroyImageView(backend->device, texture.view, nullptr);
}
