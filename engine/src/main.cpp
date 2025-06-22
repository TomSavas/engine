#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "passes/culling.h"
#include "passes/forward.h"
#include "passes/shadows.h"
#include "renderGraph.h"
#include "rhi/vulkan/backend.h"
#include "scene.h"

// #define TINYOBJLOADER_IMPLEMENTATION
// #include "tiny_obj_loader.h"

#include "tracy/Tracy.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include <chrono>
#include <cmath>
#include <optional>
#include <print>

#include "passes/testPass.h"
#include "tiny_gltf.h"

void* operator new(std::size_t size) noexcept(false)
{
    void* ptr = std::malloc(size);
    TracyAlloc(ptr, size);

    return ptr;
}

void operator delete(void* ptr)
{
    TracyFree(ptr);
    std::free(ptr);
}

struct WorldRenderer
{
    VulkanBackend& backend;
    std::optional<CompiledRenderGraph> compiledRenderGraph;

    std::optional<GeometryCulling> culling;
    std::optional<ShadowRenderer> shadows;
    std::optional<ForwardOpaqueRenderer> opaque;
    // LightCulling lightCulling;

    std::optional<TestRenderer> test;

    explicit WorldRenderer(VulkanBackend& backend) : backend(backend) {}

    void compileRenderGraph()
    {
        // NOTE: Instead of the usual per-frame recompilation of render graph,
        // it's enough to make it once and reuse the cached compiled render
        // graph. Once we get into more complicated rendering we can start
        // thinking about recompiling it every frame.
        RenderGraph graph;

        const auto culledDraws = cpuFrustumCullingPass(culling, backend, graph);
        // const auto zPrePassDepth = zPrePass(backend, graph,
        // culling.culledDraws);
        const auto shadowData = csmPass(shadows, backend, graph, 1);
        // auto lightCulling = tiledLightCullingPass(lightCulling, backend,
        // graph); auto planarReflections = planarRsflectionPass(reflections,
        // backend, graph);
        /* const auto opaqueData = */ opaqueForwardPass(
            opaque, backend, graph, culledDraws.culledDraws, shadowData.cascadeData, shadowData.shadowMap);
        // bloomPass(backend, graph);
        // reinhardTonemapPass(backend, graph);
        // smaaPass(backend, graph);

        testPass(test, backend, graph);

        compiledRenderGraph = compile(std::move(graph));
    }

    void render(Frame& frame, Scene& scene, double dt)
    {
        glfwPollEvents();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        scene.update(0.0016f, 0.f, backend.window);

        drawDebugUI(backend, scene, dt);

        // NOTE: for now let's just directly pass in the graph and let the
        // backend figure out what it wants to do. Generally we should transform
        // compiledRenderGraph into a command buffer or a list of secondary
        // command buffers. backend.recordCommandBuffer(compiledRenderGraph);
        if (compiledRenderGraph)
        {
            backend.render(frame, *compiledRenderGraph, scene);
        }
    }
};

int main()
{
    // TODO: add an allocator here
    // TODO: add multithreading solution: Main thread, GPU thread and worker
    // threads.

    // NOTE: extract VulkanBackend as interface when implementing other backends
    VulkanBackend* backend = initVulkanBackend().expect("Failed initialising Vulkan backend");

    WorldRenderer worldRenderer(*backend);
    worldRenderer.compileRenderGraph();

    // TODO: Not ideal, would be better if we could pass a lambda here
    Scene scene = loadScene(*backend, "Sponza", "../assets/Sponza/Sponza.gltf").value_or(emptyScene(*backend));

    FrameStats lastFrameStats = backend->endFrame(backend->newFrame());
    while (!lastFrameStats.shutdownRequested)
    {
        Frame frame = backend->newFrame();
        std::chrono::duration<double> elapsed = frame.stats.startTime - lastFrameStats.startTime;

        worldRenderer.render(frame, scene, elapsed.count());

        lastFrameStats = backend->endFrame(std::move(frame));
    }
    backend->deinit();

    return 0;
}
