#include "passes/atmosphere.h"

#include "debugUI.h"
#include "glm/ext/scalar_constants.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/pipelineBuilder.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"
#include "scene.h"

struct AtmospherePushConstants
{
    glm::vec4 depth;
    glm::vec4 sunDir;
    glm::vec4 scatteringCoeffs; // Rayleigh xyz, Mie w
    glm::vec4 earthAtmosphereScale;
};

auto rayleighScatteringCoefficients(f32 wavelenghts[3]) -> glm::vec3
{
    constexpr f32 n = 1.00027717f; // Air refractive index
    constexpr f32 n2 = n * n;
    constexpr f32 N = 2.504e25; // Molecular density of atmosphere -- molecules / m3
    constexpr f32 pi3 = std::pow(glm::pi<f32>(), 3.f);

    const f32 factor = 8.f * pi3 * std::pow(n2 - 1, 2.f) / 3.f * (1.f / N);
    return glm::vec3(
        factor * (1.f / std::pow(wavelenghts[0], 4.f)),
        factor * (1.f / std::pow(wavelenghts[1], 4.f)),
        factor * (1.f / std::pow(wavelenghts[2], 4.f))
    );
}

auto initAtmosphere(VulkanBackend& backend) -> std::optional<AtmosphereRenderer>
{
    return AtmosphereRenderer{
        .pipeline = PipelineBuilder(backend)
            .addDescriptorLayouts({
                backend.sceneDescriptorSetLayout,
            })
            .addPushConstants({
                VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .offset = 0,
                    .size = sizeof(AtmospherePushConstants)
                }
            })
            .addShader(SHADER_PATH("fullscreen_quad.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT)
            .addShader(SHADER_PATH("atmosphere.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT)
            .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .polyMode(VK_POLYGON_MODE_FILL)
            .cullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .disableMultisampling()
            .enableAlphaBlending()
            .colorAttachmentFormat(backend.backbufferImage.format)
            .depthFormat(VK_FORMAT_D32_SFLOAT)
            .enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
            .addViewportScissorDynamicStates()
            .build(),
    };
}

auto atmospherePass(std::optional<AtmosphereRenderer>& atmosphere, VulkanBackend& backend, RenderGraph& graph,
    RenderGraphResource<BindlessTexture> depthMap)
    -> void
{
    if (!atmosphere)
    {
        atmosphere = initAtmosphere(backend);
    }

    auto& pass = createPass(graph);
    pass.pass.debugName = "Atmosphere pass";
    pass.pass.pipeline = atmosphere->pipeline;

    depthMap = readResource<BindlessTexture>(graph, pass, depthMap, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL),

    pass.pass.beginRendering = [depthMap, &backend](VkCommandBuffer cmd, CompiledRenderGraph& graph)
    {
        VkExtent2D swapchainSize = {
            static_cast<u32>(backend.viewport.width),
            static_cast<u32>(backend.viewport.height)
        };
        auto colorAttachmentInfo = vkutil::init::renderingColorAttachmentInfo(backend.backbufferImage.view, nullptr,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto depthAttachmentInfo = vkutil::init::renderingDepthAttachmentInfo(
            backend.bindlessResources->getTexture(
                *getResource<BindlessTexture>(graph, depthMap)
            ).view,
            VK_ATTACHMENT_LOAD_OP_LOAD);
        const auto renderingInfo = vkutil::init::renderingInfo(swapchainSize, &colorAttachmentInfo, 1,
            &depthAttachmentInfo);
        vkCmdBeginRendering(cmd, &renderingInfo);
    };

    pass.pass.draw = [&backend](VkCommandBuffer cmd, CompiledRenderGraph& graph, RenderPass& pass, Scene& scene)
    {
        static f32 time = 0.f;
        static bool moveSun = false;
        static f32 speed = 0.0008f;
        static f32 sign = 1.f;
        static f32 sunIntensity = 10.f;

        static f32 rayleighScatteringWavelengths[3] = {680e-9, 550e-9, 440e-9};
        glm::vec3 rayleighCoeffs = rayleighScatteringCoefficients(rayleighScatteringWavelengths);
        const f32 mieBaseCoeff = 21e-6;
        static f32 mieFactor = 1.f;
        f32 mieCoeff = mieBaseCoeff * mieFactor;

        static f32 earthScale = 1.f;
        static f32 atmosphereScale = 1.f;
        static f32 heightScale = 1.f;
        static f32 scatteringScale = 1.f;
        static bool scalesLocked = true;

        // Seems like the static debugUI is not shared between CUs
        addDebugUI(debugUI, GRAPHICS_PASSES, [&]()
        {
            if (ImGui::TreeNode("Atmosphere"))
            {
                ImGui::SliderFloat("Time", &time, 0, 2 * glm::pi<f32>());
                ImGui::Checkbox("Sun movement", &moveSun);
                ImGui::SliderFloat("Speed", &speed, 0.00001f, 0.01f, "%.5f");

                ImGui::Separator();

                ImGui::SliderFloat("Sun Intensity", &sunIntensity, 0.f, 100.f);
                ImGui::SliderFloat3("Rayleigh scattering wavelengths", rayleighScatteringWavelengths,
                    380e-9, 700e-9, "%.8f");
                ImGui::Text("\t%.1e, %.1e, %.1e", rayleighCoeffs.x, rayleighCoeffs.y, rayleighCoeffs.z);
                ImGui::SliderFloat("Mie scattering factor", &mieFactor, 0.01f, 1000.f);
                ImGui::Text("\t%.1e", mieCoeff);

                ImGui::Separator();

                if (ImGui::SliderFloat("Earth scale", &earthScale, 0.0001f, 1.f, "%.4f") && scalesLocked)
                {
                    atmosphereScale = earthScale;
                }
                if (ImGui::SliderFloat("Atmosphere scale", &atmosphereScale, 0.0001f, 1.f, "%.4f") && scalesLocked)
                {
                    earthScale = atmosphereScale;
                }
                ImGui::Checkbox("Lock scales together", &scalesLocked);

                ImGui::SliderFloat("Height scale", &heightScale, 0.0001f, 1.f, "%.4f");
                ImGui::SliderFloat("Scattering scale", &scatteringScale, 0.0001f, 1.f, "%.4f");

                ImGui::TreePop();
            }
        });

        //if (ImGui::Begin(GRAPHICS))
        //{
        //    if (ImGui::TreeNode(GRAPHICS_PASSES))
        //    {
        //        if (ImGui::TreeNode("Atmosphere"))
        //        {
        //            ImGui::SliderFloat("Time", &time, 0, 2 * glm::pi<f32>());
        //            ImGui::Checkbox("Sun movement", &moveSun);
        //            ImGui::SliderFloat("Speed", &speed, 0.00001f, 0.01f, "%.5f");

        //            ImGui::Separator();

        //            ImGui::SliderFloat("Sun Intensity", &sunIntensity, 0.f, 100.f);
        //            ImGui::SliderFloat3("Rayleigh scattering wavelengths", rayleighScatteringWavelengths,
        //                380e-9, 700e-9, "%.8f");
        //            ImGui::Text("\t%.1e, %.1e, %.1e", rayleighCoeffs.x, rayleighCoeffs.y, rayleighCoeffs.z);
        //            ImGui::SliderFloat("Mie scattering factor", &mieFactor, 0.01f, 1000.f);
        //            ImGui::Text("\t%.1e", mieCoeff);

        //            ImGui::Separator();

        //            if (ImGui::SliderFloat("Earth scale", &earthScale, 0.0001f, 1.f, "%.4f") && scalesLocked)
        //            {
        //                atmosphereScale = earthScale;
        //            }
        //            if (ImGui::SliderFloat("Atmosphere scale", &atmosphereScale, 0.0001f, 1.f, "%.4f") && scalesLocked)
        //            {
        //                earthScale = atmosphereScale;
        //            }
        //            ImGui::Checkbox("Lock scales together", &scalesLocked);

        //            ImGui::SliderFloat("Height scale", &heightScale, 0.0001f, 1.f, "%.4f");
        //            ImGui::SliderFloat("Scattering scale", &scatteringScale, 0.0001f, 1.f, "%.4f");

        //            ImGui::TreePop();
        //        }

        //        ImGui::TreePop();
        //    }
        //}
        //ImGui::End();

        if (moveSun)
        {
            if (time > glm::pi<f32>() / 2.f + 0.2f || time < 0.f)
            {
                sign *= -1.f;
            }
            time += speed * sign;
        }
        scene.lightDir = glm::normalize(glm::vec3(-sin(time), -cos(time), 0.f));

        ZoneScopedCpuGpuAuto("Atmosphere pass", backend.currentFrame());
        const AtmospherePushConstants pushConstants = {
            .depth = glm::vec4(1.f),
            .sunDir = glm::vec4(-scene.lightDir.x, -scene.lightDir.y, scene.lightDir.z, sunIntensity),
            .scatteringCoeffs = glm::vec4(rayleighCoeffs, mieCoeff),
            .earthAtmosphereScale = glm::vec4(earthScale, atmosphereScale, heightScale, scatteringScale),
        };
        vkCmdPushConstants(cmd, pass.pipeline->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(pushConstants),
            &pushConstants);

        vkCmdDraw(cmd, 3, 1, 0, 0);
    };
}
