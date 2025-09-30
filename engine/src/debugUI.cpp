#include "debugUI.h"

#include <stack>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "imgui.h"
#include "rhi/vulkan/backend.h"
#include "scene.h"

#include <print>

DebugUI debugUI;

auto addDebugUI(DebugUI& debugUi, std::string parentId, std::function<void()> fn) -> void
{
    debugUi.fns[parentId].push_back(fn);
}

void drawDebugUI(DebugUI& debugUi, VulkanBackend& backend, Scene& scene, f64 dt)
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

    const auto drawChildren = [](const std::string& parentId)
    {
        const auto& fnIt = debugUI.fns.find(parentId);
        if (fnIt == debugUI.fns.end())
        {
            return;
        }
        for (auto& fn : fnIt->second)
        {
            fn();
        }
    };

    static bool graphicsOpen = true;
    if (ImGui::Begin(GRAPHICS_CSTR, &graphicsOpen))
    {
        if (ImGui::BeginChild(GRAPHICS_PASSES_CSTR))
        {
            drawChildren(GRAPHICS_PASSES);
            ImGui::EndChild();
        }
        drawChildren(GRAPHICS);
    }
    ImGui::End();

    static bool sceneOpen = true;
    static SceneGraph::Node* selectedNode = nullptr;
    if (ImGui::Begin(SCENE_CSTR, &sceneOpen))
    {
        std::stack<SceneGraph::Node*> nodes;
        std::stack<SceneGraph::Node*> parents;
        nodes.push(scene.sceneGraph.root);

        std::string debugS = "";

        while (!nodes.empty())
        {
            SceneGraph::Node* node = nodes.top();
            nodes.pop();

            if (node->instance != nullptr)
            {
                node->instance->selected = false;
            }

            while (!parents.empty() && parents.top() != node->parent)
            {
                ImGui::TreePop();
                parents.pop();
                debugS.pop_back();
            }

            // std::println("{}{}", debugS, node->name);
            auto selected = debugUi.selectedNode == node->name;
            auto flags = ImGuiTreeNodeFlags_None;
            if (selected)
            {
                ImGuiStyle& style = ImGui::GetStyle();
                ImVec4* colors = style.Colors;
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, colors[ImGuiCol_CheckMark]);
                flags = ImGuiTreeNodeFlags_Selected;

                selectedNode = node;

                if (selectedNode != nullptr && selectedNode->instance != nullptr)
                {
                    flags = static_cast<ImGuiTreeNodeFlags_>(flags | ImGuiTreeNodeFlags_AllowOverlap);
                    selectedNode->instance->selected = true;
                }
            }

            if (node->children.empty())
            {
                ImGui::TreeNodeEx(node->name.c_str(), flags | ImGuiTreeNodeFlags_Leaf);
                if (ImGui::IsItemClicked())
                {
                    debugUi.selectedNode = node->name;
                }
                ImGui::TreePop();
            }
            else
            {
                if (ImGui::TreeNodeEx(node->name.c_str(), flags | ImGuiTreeNodeFlags_OpenOnArrow))
                {
                    if (ImGui::IsItemClicked())
                    {
                        debugUi.selectedNode = node->name;
                    }

                    for (auto& child : node->children)
                    {
                        nodes.push(child);
                    }
                    parents.push(node);
                    debugS.push_back('\t');
                }
            }

            if (selected)
            {
                if (selectedNode != nullptr && selectedNode->instance != nullptr)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("teleport to"))
                    {
                        auto aabbSize = selectedNode->instance->aabbMax - selectedNode->instance->aabbMin;
                        // auto maxToCenter = glm::normalize(-aabbSize);
                        auto mid = (selectedNode->instance->aabbMax + selectedNode->instance->aabbMin) / 2.f;

                        glm::mat4 lookat = glm::lookAt(selectedNode->instance->aabbMax + aabbSize / 2.f, selectedNode->instance->aabbMax - aabbSize / 2.f, glm::vec3(0.f, 1.f, 0.f));
                        glm::vec3 pos;
                        glm::quat rot;
                        glm::vec3 scale;
                        glm::vec3 skew;
                        glm::vec4 perspective;
                        glm::decompose(lookat, pos, rot, scale, skew, perspective);

                        std::println("min: {} {} {}", selectedNode->instance->aabbMin.x, selectedNode->instance->aabbMin.y, selectedNode->instance->aabbMin.z);
                        std::println("max: {} {} {}", selectedNode->instance->aabbMax.x, selectedNode->instance->aabbMax.y, selectedNode->instance->aabbMax.z);
                        std::println("size: {} {} {}", aabbSize.x, aabbSize.y, aabbSize.z);
                        std::println("mid: {} {} {}", mid.x, mid.y, mid.z);

                        pos = selectedNode->instance->aabbMax + aabbSize / 2.f;
                        std::println("pos: {} {} {}", pos.x, pos.y, pos.z);

                        scene.activeCamera->position = selectedNode->instance->aabbMax + aabbSize / 2.f;
                        scene.activeCamera->rotation = toMat4(glm::conjugate(rot));
                    }
                }
                ImGui::PopStyleColor();
            }
        }
        while (!parents.empty())
        {
            ImGui::TreePop();
            parents.pop();
        }

        drawChildren(SCENE);
    }
    ImGui::End();

    static bool inspectorOpen = true;
    if (ImGui::Begin(INSPECTOR_CSTR, &inspectorOpen))
    {
        if (selectedNode != nullptr && selectedNode->instance != nullptr)
        {
            ImGui::LabelText("Selected node", "%s", selectedNode->name.c_str());
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            {
                static bool global = true;
                if (ImGui::RadioButton("Global", global))
                {
                    global = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Local", !global))
                {
                    global = false;
                }

                glm::vec3 pos;
                glm::quat rot;
                glm::vec3 scale;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(global ? selectedNode->instance->modelTransform : selectedNode->localTransform, scale, rot, pos, skew, perspective);
                glm::vec3 eulerAngles = glm::eulerAngles(glm::conjugate(rot));

                ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f, -100.f, 100.f, "%.5f");
                ImGui::DragFloat3("Rotation", glm::value_ptr(eulerAngles), 0.1f, -180.f, 180.f, "%.5f", ImGuiSliderFlags_WrapAround);
                ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.1f, -10.f, 10.f, "%.5f");

                ImGui::BeginDisabled(true);
                ImGui::DragFloat3("Min aabb", glm::value_ptr(selectedNode->instance->aabbMin), 0.1f, -10.f, 10.f, "%.5f");
                ImGui::DragFloat3("Max aabb", glm::value_ptr(selectedNode->instance->aabbMax), 0.1f, -10.f, 10.f, "%.5f");
                ImGui::EndDisabled();

                ImGui::Separator();

                ImGui::SliderFloat("Metallicness factor", &selectedNode->instance->metallicRoughnessFactors.x, 0.f, 50.f);
                ImGui::SliderFloat("Roughness factor", &selectedNode->instance->metallicRoughnessFactors.y, 0.f, 50.f);

                ImGui::Separator();
                ImGui::Text("Debug GLTF data");

                ImGui::Text("Material: %d", selectedNode->materialIndex);

            }
        }

        drawChildren(INSPECTOR);
    }
    ImGui::End();

    //if (ImGui::Begin("Output"))
    //{
    //    ImGui::Image((ImTextureID)my_texture.DS, ImVec2(my_texture.Width, my_texture.Height));
    //}
    //ImGui::End();
}
