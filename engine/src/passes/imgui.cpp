#include "passes/imgui.h"

#include "debugUI.h"
#include "imgui.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/inits.h"

#include "imgui_impl_vulkan.h"

auto initImgui(VulkanBackend& backend) -> ImguiRenderer
{
    return ImguiRenderer{
        .composite = backend.bindlessResources->addTexture(
            backend.allocateTexture(
                "Imgui output",
                vkutil::init::defaultColorAttachmentTextureCreateInfo(backend.scaledResolution),
                vkutil::init::defaultTextureAllocationCreateInfo(),
                MipOptions::one(),
                VK_IMAGE_ASPECT_COLOR_BIT
            )
        )
    };
}

auto imguiPass(std::optional<ImguiRenderer>& imguiRenderer, VulkanBackend& backend,
    RenderGraph& graph, RenderGraphResource<BindlessTexture> finalOutput)
    -> RenderGraphResource<BindlessTexture>
{
    if (!imguiRenderer)
    {
        imguiRenderer = initImgui(backend);
    }

    auto& pass = createPass(graph);
    pass.pass.debugName = std::format("Imgui pass");

    auto renderOutput = readResource<BindlessTexture>(graph, pass, finalOutput, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto output = writeResource<BindlessTexture>(graph, pass,
        importResource<BindlessTexture>(graph, pass, &imguiRenderer->composite),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    pass.pass.beginRendering = [output, &backend](const RenderContext& ctx)
    {
        // const auto& outputImage = backend.bindlessResources->getTexture(*getResource<BindlessTexture>(ctx.graph,
        //     output));
        // VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(
        //     outputImage.image.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(
            ctx.swapchain.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderingInfo = vkutil::init::renderingInfo(
            ctx.swapchain.size, &colorAttachmentInfo, 1, nullptr);

        vkCmdBeginRendering(ctx.cmd, &renderingInfo);
    };

    pass.pass.draw = [renderOutput, &backend](const RenderContext& ctx, RenderPass&)
    {
        const auto bindlessOutput = *getResource<BindlessTexture>(ctx.graph, renderOutput);
        const auto& outputTexture = backend.bindlessResources->getTexture(bindlessOutput);

        addDebugUI(debugUI, OUTPUT, [&]()
        {
            // For some reason there is a border around the image that I can't get rid of.
            // Just force the cursor position and adjust the size
            const auto padding = ImGui::GetCursorPos();
            const auto windowSize = ImGui::GetWindowContentRegionMax();
            const auto size = ImVec2(windowSize.x + padding.x, windowSize.y + padding.y);

            // imguiPos = ImGui::GetWindowPos();
            // imguiSize = size;

            ImGui::SetCursorPos(ImVec2(0, 0));
            ImGui::Image(*outputTexture.imguiDescriptorSet, size);
        }, true);

        debugDrawBindlessTextures(backend.bindlessResources.value());
        drawDebugUI(debugUI, backend, ctx.scene, 0.16);
        debugUI.fns.clear();

        ImGui::Render();

        // // TODO: perhaps we can move swapchain as a resource into render graph
        // vkutil::image::transitionImage(ctx.cmd, ctx.swapchain.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // TODO: we should probably leverage render graph for these transitions. However, currently we cannot identify
        // images + there are some resources that are not injected into render graph resource system.
        // Quite hacky, but needed for debug capabilities
        for (auto& texture : backend.bindlessResources->textures)
        {
            vkutil::image::transitionImage(ctx.cmd, texture.image.image, texture.layout,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.image.format == VK_FORMAT_D32_SFLOAT);
        }

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ctx.cmd);
    };

    return output;
}
