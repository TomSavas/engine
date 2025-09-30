#define GLFW_INCLUDE_VULKAN
#include "passes/atmosphere.h"
#include "passes/culling.h"
#include "passes/forward.h"
#include "passes/lightCulling.h"
#include "passes/shadows.h"
#include "passes/zPrePass.h"
#include "passes/blur.h"
#include "passes/bloom.h"
#include "passes/screenSpace.h"
#include "renderGraph.h"
#include "rhi/vulkan/backend.h"
#include "scene.h"
#include "debugUI.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tiny_gltf.h"
#include "GLFW/glfw3.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <chrono>
#include <optional>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

struct WorldRenderer
{
    VulkanBackend& backend;

    std::optional<CompiledRenderGraph> compiledRenderGraph;

    std::optional<GeometryCulling> culling;
    std::optional<ZPrePassRenderer> prePass;
    std::optional<ShadowRenderer> shadows;
    std::optional<ForwardOpaqueRenderer> opaque;
    std::optional<LightCulling> lightCulling;

    // Postpro fx
    std::optional<AtmosphereRenderer> atmosphere;

    std::optional<ScreenSpaceRenderer> ss;
    std::optional<BlurRenderer> blur;
    std::optional<BloomRenderer> bloom;

    explicit WorldRenderer(VulkanBackend& backend) : backend(backend) {}

    void compileRenderGraph(Scene& scene)
    {
        // NOTE: Instead of the usual per-frame recompilation of render graph,
        // it's enough to make it once and reuse the cached compiled render
        // graph. Once we get into more complicated rendering we can start
        // thinking about recompiling it every frame.
        RenderGraph graph = {
            .backend = backend
        };

        // const auto [draws, lightList] = sceneUploadPass(sceneDataUploader, backend, graph);
        const auto [culledDraws] = cpuFrustumCullingPass(culling, backend, graph);
        const auto [depthMap] = zPrePass(prePass, backend, graph, culledDraws);
        const auto [shadowMap, cascadeData] = csmPass(shadows, backend, graph, 4);
        auto lightData = tiledLightCullingPass(lightCulling, backend, graph, scene, depthMap,
            1.f / 20.f);
        // auto [lightList, culledLightData] = clusteredLightCullingPass(lightCulling, backend, graph);
        // auto [pointLightShadowAtlas] = pointLightShadowPass(pointLightShadows, backend, graph, lightList, lightIndexList, lightGrid);
        auto [colorOutput, normal, positions, reflections] = opaqueForwardPass(opaque, backend, graph, culledDraws, depthMap, cascadeData, shadowMap, lightData);
        auto output = ssrPass(ss, blur, backend, graph, colorOutput, normal, positions, reflections);
        output = atmospherePass(atmosphere, backend, graph, depthMap, output);

        auto _ = bloomPass(bloom, blur, backend, graph, output);
        //output = reinhardTonemapPass(tonemapper, backend, graph, output);
        //smaaPass(antiAliaser, backend, graph, output);

        compiledRenderGraph = compile(backend, std::move(graph));
    }

    void render(Frame& frame, Scene& scene, f64 dt)
    {
        glfwPollEvents();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

        scene.update(dt, 0.f, backend.window);

        drawDebugUI(debugUI, backend, scene, dt);
        debugUI.fns.clear();

        // NOTE: for now let's just directly pass in the graph and let the
        // backend figure out what it wants to do. Generally we should transform
        // compiledRenderGraph into a command buffer or a list of secondary
        // command buffers. I.e.: cmds = backend.recordCommandBuffers(compiledRenderGraph); backend.submit(cmds);
        if (compiledRenderGraph)
        {
            backend.render(frame, *compiledRenderGraph, scene);
        }
    }
};

i32 main()
{
    VulkanBackend* backend = initVulkanBackend().expect("Failed initialising Vulkan backend");

    Scene scene = loadScene(*backend, "Sponza", "../assets/Sponza/Sponza.gltf", 4096 - 1)
        .value_or(emptyScene(*backend));

    WorldRenderer worldRenderer(*backend);
    worldRenderer.compileRenderGraph(scene);

    FrameStats lastFrameStats = backend->endFrame(backend->newFrame());
    while (!lastFrameStats.shutdownRequested)
    {
        Frame frame = backend->newFrame();
        std::chrono::duration<f64> elapsed = frame.stats.startTime - lastFrameStats.startTime;
        frame.stats.pastFrameDt = elapsed.count();

        worldRenderer.render(frame, scene, elapsed.count());

        lastFrameStats = backend->endFrame(std::move(frame));
    }
    backend->deinit();

    return 0;
}
