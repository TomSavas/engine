#include "passes/imgui.h"

#include "debugUI.h"
#include "imgui.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/inits.h"

#include "imgui_impl_vulkan.h"

auto imguiPass(VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<BindlessTexture> renderOutput)
    -> void
{
    auto& pass = createPass(graph);
    pass.pass.debugName = std::format("Imgui pass");

    auto output = readResource<BindlessTexture>(graph, pass, renderOutput, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    pass.pass.beginRendering = [&backend](const RenderContext& ctx)
    {
        // TODO: perhaps we can move swapchain as a resource into render graph
        vkutil::image::transitionImage(ctx.cmd, ctx.swapchain.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        
        // TODO: we should probably leverage render graph for these transitions. However, currently we cannot identify
        // images + there are some resources that are not injected into render graph resource system.
        // Quite hacky, but needed for debug capabilities
        for (auto& texture : backend.bindlessResources->textures)
        {
            vkutil::image::transitionImage(ctx.cmd, texture.image.image, texture.layout,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.image.format == VK_FORMAT_D32_SFLOAT);
        }

        VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(
            ctx.swapchain.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(
            ctx.swapchain.size, &colorAttachmentInfo, 1, nullptr);

        vkCmdBeginRendering(ctx.cmd, &renderingInfo);
    };

    pass.pass.draw = [output, &backend](const RenderContext& ctx, RenderPass&)
    {
        const auto bindlessOutput = *getResource<BindlessTexture>(ctx.graph, output);
        const auto& outputTexture = backend.bindlessResources->getTexture(bindlessOutput);

        addDebugUI(debugUI, OUTPUT, [&]()
        {
            ImGui::Image(*outputTexture.imguiDescriptorSet, ImGui::GetContentRegionAvail());
        }, true);
            
        // Debug UI
        debugDrawBindlessTextures(backend.bindlessResources.value());
        drawDebugUI(debugUI, backend, ctx.scene, ctx.frame.stats.pastFrameDt);
        debugUI.fns.clear();

        ImGui::Render();

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ctx.cmd);
    };
}
