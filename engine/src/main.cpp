#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "scene.h"
#include "passes/passes.h"
#include "rhi/vulkan/backend.h"

// #define TINYOBJLOADER_IMPLEMENTATION
// #include "tiny_obj_loader.h"

#include "tracy/Tracy.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tiny_gltf.h"

#include <chrono>
#include <print>
#include <math.h>
#include <optional>

// TEMP: test for tracy allocations
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
    RHIBackend& backend;
    std::optional<CompiledRenderGraph> compiledRenderGraph;

    std::optional<GeometryCulling> culling;
    std::optional<ShadowRenderer> shadows;
    std::optional<ForwardOpaqueRenderer> opaque;
    // LightCulling lightCulling;

    explicit WorldRenderer(RHIBackend& backend) : backend(backend)
    {
    }

    void compileRenderGraph()
    {
        // NOTE: Instead of the usual per-frame recompilation of render graph, it's enought to make
        // it once and reuse the cached compiled render graph. Once we get into more complicated
        // rendering we can start thinking about recompiling it every frame.
        RenderGraph graph;

        const auto culledDraws = cpuFrustumCullingPass(culling, backend, graph);
        // const auto zPrePassDepth = zPrePass(backend, graph, culling.culledDraws);
        const auto shadowData = csmPass(shadows, backend, graph);
        // auto lightCulling = tiledLightCullingPass(lightCulling, backend, graph);
        // auto planarReflections = planarRsflectionPass(reflections, backend, graph);
        const auto opaqueData = opaqueForwardPass(backend, graph, culledDraws.culledDraws,
            shadowData.cascadeData, shadowData.shadowMap);
        // bloomPass(backend, graph);
        // reinhardTonemapPass(backend, graph);
        // smaaPass(backend, graph);

        // auto culledDraws = cullingPass(backend, graph);
        // auto depthStencil = zPrePass(backend, backend.graph, scene);
        // auto shadowData = shadowPass(backend, graph);
        // basePass(backend, graph, culledDraws, shadowData);
        // basePass(backend, graph, culledDraws.culledDraws, shadowData.cascadeData, shadowData.shadowMap);

        compiledRenderGraph = compile(std::move(graph));
    }

    void render(Frame& frame, Scene& scene, double dt)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        scene.update(0.0016f, 0.f, backend.window);

        drawDebugUI(backend, scene, dt);

        // NOTE: for now let's just directly pass in the graph and let the backend figure out what it
        // wants to do. Generally we should transform compiledRenderGraph into a command buffer or a
        // list of secondary command buffers.
        // backend.recordCommandBuffer(compiledRenderGraph);
        if (compiledRenderGraph)
        {
            backend.recordCommandBuffers(scene, compiledRenderGraph);
        }
    }
};

int main(void) 
{
    // TODO: add an allocator here
    // TODO: add multithreading solution: Main thread, GPU thread and worker threads. 

    // NOTE: extract RHIBackend as interface when implementing other backends
    std::optional<RHIBackend> maybeBackend = initVulkanBackend();
    if (!maybeBackend)
    {
        std::println("Failed to initialize backend");
        return 0;
    }
    RHIBackend backend = std::move(*maybeBackend);

    WorldRenderer worldRenderer = WorldRenderer(backend);
    RenderGraph renderGraph;

    // TODO: should be a task for IO threads
    Scene scene = Scene("Sponza", backend);
    scene.load("../assets/Sponza/Sponza.gltf");

    Frame lastFrame = backend.newFrame();
    backend.endFrame(lastFrame);
    while (!backend.shutdownRequested)
    {
        Frame frame = backend.newFrame();
        std::chrono::duration<double> elapsed = frame.startTime - lastFrame.startTime;

        glfwPollEvents();

        worldRenderer.render(frame, scene, elapsed.count());
        backend.endFrame(frame);
        lastFrame = frame;
    }
    backend.deinit();

    return 0;
}
