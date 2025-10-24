#include "rhi/vulkan/bindless.h"

#include "debugUI.h"
#include "imgui_impl_vulkan.h"
#include "renderGraph.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/descriptors.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/utils/texture.h"

#include <cmath>
#include <random>
#include <print>

BindlessResources::BindlessResources(VulkanBackend& backend) : backend(&backend)
{
    constexpr u32 maxBindlessResourceCount = 10000;
    capacity = maxBindlessResourceCount;
    {
        VkDescriptorPoolSize poolSizes[] = {
            VkDescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = maxBindlessResourceCount},
        };
        bindlessDescPoolAllocator.init(
            backend.device, maxBindlessResourceCount, poolSizes, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
    }

    constexpr VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                       VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                       VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorSetCount = 1,
        // TODO: This should be an actual array that actually depends on how many descriptors we have
        // For now it's only textures tho.
        .pDescriptorCounts = &maxBindlessResourceCount
    };

    DescriptorSetLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindlessFlags, maxBindlessResourceCount);
    // TODO: Add more bindings for buffers, etc.
    bindlessTexDescLayout = builder.build(
        backend.device, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    bindlessTexDesc = bindlessDescPoolAllocator.allocate(
        backend.device, bindlessTexDescLayout, &variableDescriptorCountAllocInfo);

    // Default textures. No need to deallocate -- we need these to always exist
    addTexture(whiteTexture(backend, 1));
    addTexture(blackTexture(backend, 1));
    addTexture(errorTexture(backend, 64));
}

auto BindlessResources::addTexture(Texture texture) -> BindlessTexture
{
    // TODO: move this out somewhere else:
    if (texture.name.empty())
    {
        // Some nonsense to generate "unique" name

        static std::random_device dev;
        static std::mt19937 rng(dev());
        std::uniform_int_distribution<int> dist(0, 9);

        texture.name = "generated_";
        texture.name.reserve(10 + 128);
        for (u32 i = 0; i < 128; i++)
        {
            texture.name += std::to_string(dist(rng));
        }
    }

    if (cache.contains(texture.name))
    {
        return cache[texture.name];
    }
    //
    // TEMP: this is really shitty and should be packaged somewhere else
    if (texture.sampler == VK_NULL_HANDLE)
    {
        VkSamplerCreateInfo samplerInfo = vkutil::init::samplerCreateInfo(
            VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, static_cast<f32>(texture.mipCount));
        vkCreateSampler(backend->device, &samplerInfo, nullptr, &texture.sampler);
    }

    // TODO: Updating the bindless texture data should be moved
    VkDescriptorImageInfo descriptorImageInfo = vkutil::init::descriptorImageInfo(
        texture.sampler, texture.image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkWriteDescriptorSet descriptorWrite = vkutil::init::writeDescriptorImage(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindlessTexDesc, &descriptorImageInfo, 0);

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

    cache[texture.name] = index;

    return index;
}

auto BindlessResources::getTexture(BindlessTexture handle, BindlessTexture defaultTexture) -> Texture&
{
    if (handle < textures.size() && !freeIndices.contains(handle)) return textures[handle];

    return textures[defaultTexture];
}

auto BindlessResources::removeTexture(BindlessTexture handle) -> void { assert(false); }

auto imageLayoutToString(VkImageLayout layout) -> const char*
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return "VK_IMAGE_LAYOUT_UNDEFINED";
        case VK_IMAGE_LAYOUT_GENERAL:
            return "VK_IMAGE_LAYOUT_GENERAL";
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return "VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            return "VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL";
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return "VK_IMAGE_LAYOUT_PREINITIALIZED";
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
            return "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
            return "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
            return "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
            return "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
            return "VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
            return "VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
            return "VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
            return "VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ:
            return "VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ";
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR";
        case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
            return "VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR";
        case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
            return "VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT";
        case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
            return "VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR";
        case VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT:
            return "VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_QUANTIZATION_MAP_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_QUANTIZATION_MAP_KHR";
        case VK_IMAGE_LAYOUT_MAX_ENUM:
            return "VK_IMAGE_LAYOUT_MAX_ENUM";
        default:
            return "Unknown layout";
    }
}

template <>
auto addTransition<BindlessTexture>(VulkanBackend& backend, CompiledRenderGraph::Node& node, BindlessTexture* resource,
    Layout oldLayout, Layout newLayout)
    -> void
{
    auto texture = backend.bindlessResources->getTexture(*resource);
    // if (oldLayout == newLayout)
    // {
    //     std::print("[SKIPPED] ");
    // }
    std::println("[{}] transitioning image {} (0x{:x}) {} -> {}", node.pass.debugName, texture.name, (u64)texture.image.image, imageLayoutToString(oldLayout), imageLayoutToString(newLayout));

    // if (oldLayout == newLayout)
    // {
    //     return;
    // }

    VkImageMemoryBarrier2 imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier.pNext = nullptr;

    // TODO: we can improve this
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageBarrier.oldLayout = oldLayout;
    imageBarrier.newLayout = newLayout;

    auto& tex = backend.bindlessResources->getTexture(*resource);
    imageBarrier.image = tex.image.image;

    const bool isDepth = tex.image.format == VK_FORMAT_D32_SFLOAT;

    VkImageAspectFlags aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange = vkutil::init::imageSubresourceRange(aspectMask);

    tex.layout = newLayout;

    node.imageBarriers.push_back(imageBarrier);
}

auto debugDrawBindlessTextures(BindlessResources& bindlessResources) -> void
{
    static std::vector<BindlessTexture> textureWindows;

    addDebugUI(debugUI, RESOURCES, [&]()
    {
        constexpr auto textureSize = 64;
        const auto size = ImGui::GetContentRegionAvail();
        auto columns = std::floor(size.x / textureSize);

        ImGui::Text("Bindless texture capacity: %d", bindlessResources.capacity);
        ImGui::Text("Total loaded: %d - %d = %d", bindlessResources.lastUsedIndex, bindlessResources.freeIndices.size(), bindlessResources.lastUsedIndex - bindlessResources.freeIndices.size());

        auto drawImages = [&](const char* tableId, bool loadedFromFileFilter)
        {
            ImGui::BeginTable(tableId, std::max<u32>(1, columns));
            ImGui::TableNextColumn();
            for (u32 i = 0; i < bindlessResources.textures.size(); i++)
            {
                Texture& texture = bindlessResources.textures[i];
                if (texture.loadedFromFile != loadedFromFileFilter)
                {
                    continue;
                }

                if (!texture.imguiDescriptorSet)
                {
                    texture.imguiDescriptorSet = ImGui_ImplVulkan_AddTexture(texture.sampler, texture.image.view,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }

                ImGui::Image(*texture.imguiDescriptorSet, ImVec2(textureSize, textureSize));
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
                {
                    if (ImGui::BeginItemTooltip())
                    {
                        ImGui::Text("Name: %s", texture.name.c_str());
                        ImGui::Text("Size: %d x %d x %d", texture.image.extent.width, texture.image.extent.height, texture.image.extent.depth);
                        ImGui::Text("Mips: %d", texture.mipCount);
                        ImGui::Text("Bindless index: %d", i);

                        const auto isFree = bindlessResources.freeIndices.contains(i);
                        if (isFree)
                        {
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(255, 0, 0, 255), "Free");
                        }

                        ImGui::Separator();
                        const auto aspectRatio = static_cast<f32>(texture.image.extent.width) / static_cast<f32>(texture.image.extent.height);
                        ImVec2 size;
                        if (aspectRatio > 1.f)
                        {
                            size = ImVec2(512, 512.f / aspectRatio);
                        }
                        else
                        {
                           size = ImVec2(512.f * aspectRatio, 512);
                        }
                        ImGui::Image(*texture.imguiDescriptorSet, size);
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::IsItemClicked() && !std::ranges::contains(textureWindows, i))
                {
                    textureWindows.push_back(i);
                }

                ImGui::Text("%d", i);
                ImGui::TableNextColumn();
            }
            ImGui::EndTable();

        };

        if (ImGui::CollapsingHeader("Runtime resources", ImGuiTreeNodeFlags_DefaultOpen))
        {
            drawImages("Generated bindless resources", false);
        }
        if (ImGui::CollapsingHeader("Loaded resources", ImGuiTreeNodeFlags_DefaultOpen))
        {
            drawImages("Loaded bindless resources", true);
        }
    });

    addDebugUI(debugUI, GLOBAL, [&]()
    {
        for (u32 i = 0; i < textureWindows.size(); ++i)
        {
            const auto tex = textureWindows[i];
            const auto& texture = bindlessResources.textures[tex];

            bool open = true;
            ImGui::SetNextWindowSize(ImVec2(256, 256), ImGuiCond_Once);
            if (ImGui::Begin(texture.name.c_str(), &open, ImGuiWindowFlags_NoCollapse))
            {
                ImGui::Text("Name: %s", texture.name.c_str());
                ImGui::Text("Size: %d x %d x %d", texture.image.extent.width, texture.image.extent.height, texture.image.extent.depth);
                ImGui::Text("Mips: %d", texture.mipCount);
                ImGui::Text("Bindless index: %d", tex);
                ImGui::Separator();
                ImGui::Image(*texture.imguiDescriptorSet, ImGui::GetContentRegionAvail());
            }

            if (!open)
            {
                textureWindows[i] = textureWindows.back();
                textureWindows.pop_back();
                i -= 1;
            }

            ImGui::End();
        }
    });
}
