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

using RHIBackend = VulkanBackend;

struct WorldRenderer
{
    RHIBackend& backend;
    
    explicit WorldRenderer(RHIBackend& backend) : backend(backend) {}
    
    void render(Frame& frame, Scene& scene, double dt)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawDebugUI(backend, scene, dt);
        
        scene.update(0.0016f, 0.f, backend.window);
        backend.draw(scene);
    }
};

int main(void) 
{
    // TODO: add an allocator here
    // TODO: add multithreading solution: Main thread, GPU thread and worker threads. 

    // NOTE: extract RHIBackend as interface when implementing other backends
    RHIBackend backend;
    // TODO: convert this to result type
    if (std::optional<RHIBackend> maybeBackend = initVulkanBackend())
    {
        backend = std::move(*maybeBackend);
    }
    else
    {
        std::println("Failed to initialize backend");
        return 0;
    }

    WorldRenderer worldRenderer = WorldRenderer(backend);
    RenderGraph renderGraph;

    Scene scene = Scene("Sponza");
    // TODO: should be a task for IO threads
    scene.load("../assets/Sponza/Sponza.gltf");

    // TODO: constructors should not depend on the scene and should be done in WorldRenderer
    {
        auto culledDraws = cullingPass(backend, backend.graph, scene);
        // auto depthStencil = zPrePass(backend, backend.graph, scene);
        auto shadowData = shadowPass(backend, backend.graph, scene);
        basePass(backend, backend.graph, scene, culledDraws, shadowData);
    }
    
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
