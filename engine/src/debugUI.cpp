#include "imgui.h"
#include "rhi/vulkan/backend.h"
#include "scene.h"

void drawDebugUI(VulkanBackend& backend, Scene& scene, f64 dt)
{
    constexpr f32 padding = 10.0f;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                    ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;  // Use work area to avoid menu-bar/task-bar, if any!
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 window_pos;
    window_pos.x = work_pos.x + work_size.x - padding;
    window_pos.y = work_pos.y + padding;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1.f, 0.f));

    static bool open = false;
    constexpr i32 sampleCount = 1024;
    static f64 frameTimes[sampleCount];
    static i32 frameTimesIdx = 0;
    if (!open)
    {
        for (i32 i = 0; i < sampleCount; ++i)
        {
            frameTimes[i] = dt;
        }
        open = true;
    }

    frameTimes[frameTimesIdx] = dt;
    f64 avgFrameTime = dt;
    for (i32 i = (frameTimesIdx + 1) % sampleCount; i != frameTimesIdx; i = (i + 1) % sampleCount)
    {
        avgFrameTime += frameTimes[i];
    }
    avgFrameTime /= static_cast<f32>(sampleCount);
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

        ImGui::Text("CPU: %.3f ms", dt * 1000.f);
        ImGui::Text("     %.3f Hz", 1.0 / dt);
        ImGui::Text("--- Avg ---");
        ImGui::Text("CPU: %.3f ms", avgFrameTime * 1000.f);
        ImGui::Text("     %.3f Hz", 1.0 / avgFrameTime);

        ImGui::Separator();
        ImGui::Text("Pos: %.2f, %.2f, %.2f", scene.activeCamera->position.x, scene.activeCamera->position.y,
            scene.activeCamera->position.z);

        glm::vec3 fw = scene.activeCamera->rotation * glm::vec4(0.f, 0.f, -1.f, 0.f);
        glm::vec3 r = scene.activeCamera->rotation * glm::vec4(1.f, 0.f, 0.f, 0.f);
        glm::vec3 u = scene.activeCamera->rotation * glm::vec4(0.f, 1.f, 0.f, 0.f);
        ImGui::Text("Forward: %.2f, %.2f, %.2f", fw.x, fw.y, fw.z);
        ImGui::Text("Right: %.2f, %.2f, %.2f", r.x, r.y, r.z);
        ImGui::Text("Up: %.2f, %.2f, %.2f", u.x, u.y, u.z);

        ImGui::Text("Active camera: %s", (scene.activeCamera == &scene.mainCamera) ? "main" : "debug");
        ImGui::Text("Movement speed: %.2f", scene.activeCamera->moveSpeed);
    }
    ImGui::End();
}
