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


int main(void) {
    if (!glfwInit()) {
        std::println("Failed initing GLFW");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Engine", NULL, NULL);

    VulkanBackend backend(window);
    backend.registerCallbacks();

    Scene scene = Scene("empty");

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    // const char* modelPath = "../assets/Box/Box.gltf"; 
    const char* modelPath = "../assets/Sponza/Sponza.gltf"; 
    // const char* modelPath = "../assets/Suzanne/Suzanne.gltf"; 
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, modelPath);
    if (!ret) {
        std::println("{}", err);
        std::println("{}", warn);
    } else {
        std::println("Successfully loaded {}", modelPath);
    }

    scene.addMeshes(model);


    {
        // backend.graph.renderpasses.push_back(infGrid(backend).value_or(emptyPass(backend)));
        backend.graph.renderpasses.push_back(basePass(backend, scene).value_or(emptyPass(backend)));
        // backend.graph.renderpasses.push_back(*infGrid(backend));
        // backend.graph.renderpasses.push_back(emptyPass(backend));
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    float totalTimeElapsed = 0.f;
    while(!glfwWindowShouldClose(window)) 
    {
        end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        double dt = (double)elapsed.count() / 1000000000.0;
        totalTimeElapsed += dt;
        start = std::chrono::high_resolution_clock::now();

        glfwPollEvents();

        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
        }

        scene.update(dt, totalTimeElapsed, window);

        {
            const float PAD = 10.0f;

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
            ImVec2 work_size = viewport->WorkSize;
            ImVec2 window_pos;
            window_pos.x = work_pos.x + work_size.x - PAD;
            window_pos.y = work_pos.y + PAD;
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1.f, 0.f));

            static bool open = false;
            constexpr int sampleCount = 1024;
            static double frameTimes[sampleCount];
            static int frameTimesIdx = 0;
            if (!open) {
                for (int i = 0; i < sampleCount; ++i) {
                    frameTimes[i] = dt;
                }
                open = true;
            }

            frameTimes[frameTimesIdx] = dt;
            double avgFrameTime = dt;
            for (int i = (frameTimesIdx + 1) % sampleCount; i != frameTimesIdx; i = (i + 1) %  sampleCount) {
                avgFrameTime += frameTimes[i];
            }
            avgFrameTime /= static_cast<float>(sampleCount);
            frameTimesIdx = (frameTimesIdx + 1) % sampleCount;

            ImGui::SetNextWindowBgAlpha(0.75f);
            if (ImGui::Begin("Info", &open, window_flags))
            {
                ImGui::Text("Engine");
                ImGui::SameLine();

#ifdef DEBUG
                ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "DEBUG");
#else
                ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "RELEASE");
#endif
                ImGui::Separator();
                ImGui::Text("Frames: %.ld", backend.stats.finishedFrameCount);
                ImGui::Separator();
                
                ImGui::Text("CPU: %.9f msec", dt * 1000.f);
                ImGui::Text("     %.9f Hz", 1.0 / dt);
                ImGui::Text("--- Avg ---");
                ImGui::Text("CPU: %.9f msec", avgFrameTime * 1000.f);
                ImGui::Text("     %.9f Hz", 1.0 / avgFrameTime);

                ImGui::Separator();
                ImGui::Text("Pos: %.2f, %.2f, %.2f", scene.activeCamera.position.x, scene.activeCamera.position.y, scene.activeCamera.position.z);

                glm::vec3 fw = scene.activeCamera.rotation * glm::vec4(0.f, 0.f, -1.f, 0.f);
                glm::vec3 r = scene.activeCamera.rotation * glm::vec4(1.f, 0.f, 0.f, 0.f);
                glm::vec3 u = scene.activeCamera.rotation * glm::vec4(0.f, 1.f, 0.f, 0.f);
                ImGui::Text("Forward: %.2f, %.2f, %.2f", fw.x, fw.y, fw.z);
                ImGui::Text("Right: %.2f, %.2f, %.2f", r.x, r.y, r.z);
                ImGui::Text("Up: %.2f, %.2f, %.2f", u.x, u.y, u.z);

                ImGui::Text("");
                ImGui::Text("Movement speed: %.2f", scene.activeCamera.moveSpeed);
            }
            ImGui::End();
        }
        ImGui::Render();
        backend.draw(scene);
    }

    backend.deinit();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
