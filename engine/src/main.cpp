#define GLFW_INCLUDE_VULKAN
#include "passes/atmosphere.h"
#include "passes/culling.h"
#include "passes/forward.h"
#include "passes/lightCulling.h"
#include "passes/shadows.h"
#include "passes/testPass.h"
#include "passes/zPrePass.h"
#include "renderGraph.h"
#include "rhi/vulkan/backend.h"
#include "scene.h"
#include "debugUI.h"
#include "sceneGraph.h"

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
#include "passes/screenSpace.h"

struct WorldRenderer
{
    VulkanBackend& backend;
    std::optional<CompiledRenderGraph> compiledRenderGraph;

    std::optional<GeometryCulling> culling;
    std::optional<ZPrePassRenderer> prePass;
    std::optional<ShadowRenderer> shadows;
    std::optional<ForwardOpaqueRenderer> opaque;
    std::optional<LightCulling> lightCulling;

    std::optional<AtmosphereRenderer> atmosphere;

    // Postpro fx
    std::optional<ScreenSpaceRenderer> ss;


    std::optional<TestRenderer> test;

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
        // auto [pointLightShadowAtlas] = pointLightShadowPass(pointLightShadows, backend, graph, lightList, lightIndexList, lightGrid);
        // auto [lightList, culledLightData] = clusteredLightCullingPass(lightCulling, backend, graph);
        // auto planarReflections = planarReflectionPass(reflections, backend, graph);
        const auto [colorOutput, normal, positions, reflections] = opaqueForwardPass(opaque, backend, graph, culledDraws, depthMap, cascadeData, shadowMap, lightData);
        auto _ = ssrPass(ss, backend, graph, colorOutput, normal, positions, reflections);
        // bloomPass(backend, graph);
        // reinhardTonemapPass(backend, graph);
        // smaaPass(backend, graph);
        atmospherePass(atmosphere, backend, graph, depthMap); //, output);

        //testPass(test, backend, graph);

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

    //Scene scene = loadScene(*backend, "Sponza", "../assets/Suzanne/Suzanne.gltf", 1)
    Scene scene = loadScene(*backend, "Sponza", "../assets/Sponza/Sponza.gltf", 1024 - 1)
    //Scene scene = loadScene(*backend, "Sponza", "../assets/VC/VC.gltf", 1024 - 1)
    //Scene scene = loadScene(*backend, "Sponza", "../assets/intelsponza/sponza.gltf", 1)
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
