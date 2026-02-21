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
#include "passes/sceneData.h"
#include "passes/imgui.h"
#include "renderGraph.h"
#include "rhi/vulkan/backend.h"
#include "scene.h"
#include "debugUI.h"
#include "debugShapes.h"
#include "ImGuizmo.h"

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
#include <glm/ext/matrix_transform.hpp>

#include <print>
#include <array>

#include "scenes.h"

template <typename T>
concept Renderer = requires(T t)
{
    { t.enabled() } -> std::same_as<bool>;
};

struct WorldRenderer
{
    VulkanBackend& backend;

    std::optional<CompiledRenderGraph> compiledRenderGraph;

    std::optional<SceneDataUploader> sceneDataUploader;

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

    RenderGraphResource<BindlessTexture> output;

    explicit WorldRenderer(VulkanBackend& backend) : backend(backend) {}

    void compileRenderGraph(Scene& scene)
    {
        RenderGraph graph = {
            .backend = backend
        };

        const auto [draws, lightList, perModelData] = sceneUploadPass(sceneDataUploader, backend, graph, scene);
        // const auto [culledDraws] = cpuFrustumCullingPass(culling, backend, graph);
        const auto culledDraws = draws; // TODO: temporarily disable culling while rewriting scene
        const auto [depthMap] = zPrePass(prePass, backend, graph, culledDraws, perModelData);
        const auto [shadowMap, cascadeData] = csmPass(shadows, backend, graph, 4, perModelData, draws);
        auto lightData = tiledLightCullingPass(lightCulling, backend, graph, scene, depthMap,
            1.f / 20.f);
        auto [colorOutput, normal, positions, reflections] = opaqueForwardPass(opaque, backend, graph, culledDraws, depthMap, cascadeData, shadowMap, lightData, perModelData);
        output = ssrPass(ss, blur, backend, graph, colorOutput, normal, positions, reflections);
        output = atmospherePass(atmosphere, backend, graph, depthMap, output);
        output = bloomPass(bloom, blur, backend, graph, output);
        // output = colorOutput;
        //output = reinhardTonemapPass(tonemapper, backend, graph, output);
        //smaaPass(antiAliaser, backend, graph, output);

        if (debugUI.enabled)
        {
            imguiPass(backend, graph, output);
        }
        else
        {
            // TODO: perhaps this can be removed once the rg is smart enough for marking the output buffer
            backend.addOutputBlitPass(graph, output);
        }

        compiledRenderGraph = compile(backend, std::move(graph));
    }

    void render(Frame& frame, Scene& scene, f64 dt)
    {
        compileRenderGraph(scene);
        
        scene.update(dt, 0.f, backend.window, !debugUI.enabled || debugUI.outputFocused);

        {
            static auto debugKeyWasPressed = false;
            auto debugKeyPressed = glfwGetKey(backend.window, GLFW_KEY_F1) == GLFW_PRESS;
            if (debugKeyWasPressed && !debugKeyPressed)
            {
                debugUI.enabled = !debugUI.enabled;
            }
            debugKeyWasPressed = debugKeyPressed;
        }

        // Debug UI
        debugDrawBindlessTextures(backend.bindlessResources.value());
        drawDebugUI(debugUI, backend, scene, dt);
        debugUI.fns.clear();

        // NOTE: for now let's just directly pass in the graph and let the
        // backend figure out what it wants to do. Generally we should transform
        // compiledRenderGraph into a command buffer or a list of secondary
        // command buffers. I.e.: cmds = backend.recordCommandBuffers(compiledRenderGraph); backend.submit(cmds);
        if (compiledRenderGraph)
        {
            backend.render(frame, *compiledRenderGraph, scene, output);
        }
    }
};

i32 main()
{
    VulkanBackend* backend = initVulkanBackend().expect("Failed initialising Vulkan backend");

    Scene scene = physicsZoo(*backend);
    // Scene scene = sponzaScene(*backend);
    // Scene scene = instancingTestScene(*backend);

    debugDrawCube(scene, glm::vec3(0.f, 2.5f, 0.f), glm::vec3(1.f), glm::vec3(1.f, 0.f, 0.f));
    debugDrawCube(scene, glm::vec3(0.f, 5.5f, 0.f), glm::vec3(1.f), glm::vec3(1.f, 0.f, 0.f));
    debugDrawCube(scene, glm::vec3(1.f, 2.f, 3.f), glm::vec3(1.f), glm::vec3(1.f, 0.f, 0.f));

    WorldRenderer worldRenderer(*backend);

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
