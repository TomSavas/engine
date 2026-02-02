
#include "debugUI.h"

#include "rhi/vulkan/backend.h"
#include "scene.h"
#include "ImGuizmo.h"
#include "IconsFontAwesome5.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "GLFW/glfw3.h"

#include <print>
#include <stack>


DebugUI debugUI;

auto addDebugUI(DebugUI& debugUI, std::string parentId, std::function<void()> fn, bool prepend) -> void
{
    if (prepend)
    {
        debugUI.fns[parentId].insert(debugUI.fns[parentId].begin(), fn);
    }
    else
    {
        debugUI.fns[parentId].push_back(fn);
    }
}

auto drawDebugUI(DebugUI& debugUI, VulkanBackend& backend, Scene& scene, f64 dt) -> void
{
    if (!debugUI.enabled)
    {
        return;
    }
    
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

            ImGui::Separator();

            VmaBudget budgets[8];
            vmaGetHeapBudgets(backend.allocator, budgets);
            ImGui::Text("Total VRAM: %lf", static_cast<f64>(budgets[0].budget) / (1024.f * 1024.f));
            ImGui::Text("Using VRAM: %lf", static_cast<f64>(budgets[0].usage) / (1024.f * 1024.f));
            f64 load = static_cast<f64>(budgets[0].usage) / static_cast<f64>(budgets[0].budget);
            ImGui::Text("VRAM load: %lf %%", load * 100.0);
        }
        ImGui::End();
    }

    const auto drawChildren = [&](const std::string& parentId)
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
        }
        ImGui::EndChild();
        drawChildren(GRAPHICS);
    }
    ImGui::End();
    
    static SceneGraph::NodeHandle selectedHandle = SceneGraph::kRootHandle;

    // Unhighlight
    {
        auto& node = scene.sceneGraph.nodes[selectedHandle];
        if (node.model && node.instance)
        {
            int materialHandle = static_cast<int>(scene.models.instances[*node.model][*node.instance].material.x);
            DefaultMaterial& material = scene.materials.materials[materialHandle];
            material.features = material.features & (DefaultMaterial::Features)~(u64)DefaultMaterial::Features::HIGHLIGHT;
        }
    }
    
    if (ImGui::Begin(SCENE_CSTR, nullptr))
    {
        ImGui::Text("Currently selected: %d", selectedHandle);
        
        std::unordered_map<SceneGraph::NodeHandle, std::vector<SceneGraph::NodeHandle>> hierarchy;

        for (size_t i{}; i < scene.sceneGraph.nodes.size(); ++i)
        {
            auto& node = scene.sceneGraph.nodes[i];
            if (node.parent != i)
            {
                hierarchy[node.parent].push_back(i);
            }
        }

        const std::function<void(SceneGraph::NodeHandle)> showNode = [&](SceneGraph::NodeHandle handle)
            {
                const auto& node = scene.sceneGraph.nodes[handle];

                auto flags = ImGuiTreeNodeFlags_DrawLinesFull |
                    (selectedHandle == handle ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None);
                const auto hasChildren = hierarchy[handle].size() != 0;
                if (hasChildren)
                {
                    flags = static_cast<ImGuiTreeNodeFlags_>(flags | ImGuiTreeNodeFlags_OpenOnArrow);
                }
                else
                {
                    flags = static_cast<ImGuiTreeNodeFlags_>(flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet);
                }

                const auto nodeOpen = ImGui::TreeNodeEx(node.name.c_str(), flags);
                if (ImGui::IsItemClicked())
                {
                    selectedHandle = handle;
                }

                if (nodeOpen)
                {
                    for (auto child : hierarchy[handle])
                    {
                        showNode(child);
                    }
                    ImGui::TreePop();
                }
            };

        showNode(SceneGraph::kRootHandle);
    }
    ImGui::End();

    // Highlight
    {
        auto& node = scene.sceneGraph.nodes[selectedHandle];
        if (node.model && node.instance)
        {
            int materialHandle = static_cast<int>(scene.models.instances[*node.model][*node.instance].material.x);
            DefaultMaterial& material = scene.materials.materials[materialHandle];
            material.features = material.features | DefaultMaterial::Features::HIGHLIGHT;
        }
    }

    if (ImGui::Begin(INSPECTOR_CSTR, nullptr))
    {
        auto& node = scene.sceneGraph.nodes[selectedHandle];
        ImGui::Text("Selected node (%03d): %s", selectedHandle, node.name.c_str());

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (selectedHandle == 0)
                ImGui::BeginDisabled(true);
            
            static bool global = true;
            const bool selectedRoot = selectedHandle == SceneGraph::kRootHandle;
            if (selectedRoot)
            {
                ImGui::BeginDisabled(true);
                global = true;
            }
            if (ImGui::RadioButton("Global", global))
            {
                global = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Local", !global))
            {
                global = false;
            }
            if (selectedRoot)
            {
                ImGui::EndDisabled();
            }

            glm::vec3 pos;
            glm::quat rot;
            glm::vec3 scale;
            glm::vec3 skew;
            glm::vec4 perspective;
            if (global)
            {
                glm::decompose(node.globalTransform, scale, rot, pos, skew, perspective);
            }
            else
            {
                glm::decompose(node.localTransform, scale, rot, pos, skew, perspective);
            }
            glm::vec3 eulerAngles = glm::eulerAngles(glm::conjugate(rot));

            glm::vec3 posCopy = pos;

            auto changed = ImGui::SliderFloat3("Position", glm::value_ptr(pos), -100.f, 100.f, "%f");
            changed |= ImGui::SliderFloat3("Rotation", glm::value_ptr(eulerAngles), -180.f, 180.f, "%f");
            ImGui::BeginDisabled(true);
            ImGui::SliderFloat3("Scale", glm::value_ptr(scale), -10.f, 10.f, "%f");
            ImGui::EndDisabled();
            float singleScale = scale.x;
            changed |= (ImGui::SliderFloat("Scale", &singleScale, -10.f, 10.f, "%f") && singleScale != 0);
            scale = glm::vec3(singleScale);

            if (changed)
            {
                // INVERTED! We need to update the counterpart of what we changed
                node.dirty = global
                    ? SceneGraph::NewNode::TransformDirtiness::LOCAL_DIRTY
                    : SceneGraph::NewNode::TransformDirtiness::GLOBAL_DIRTY;

                glm::quat actualRot = glm::quat(glm::radians(eulerAngles));
                // If you used conjugate in the UI display, ensure this matches your math:
                actualRot = glm::conjugate(actualRot);
                glm::mat4 newlyBuilt = glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(actualRot) * glm::scale(glm::mat4(1.0f), scale);
                
                if (global)
                {
                    node.globalTransform = newlyBuilt;
                }
                else
                {
                    node.localTransform = newlyBuilt;
                }
            }

            if (selectedHandle == 0)
                ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto drawTexture = [&](int textureHandle)
                {
                    Texture& texture = backend.bindlessResources->textures[textureHandle];

                    constexpr auto textureSize = 64;
                    ImGui::Image(*texture.imguiDescriptorSet, ImVec2(textureSize, textureSize));
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
                    {
                        if (ImGui::BeginItemTooltip())
                        {
                            ImGui::Text("Name: %s", texture.name.c_str());
                            ImGui::Text("Size: %d x %d x %d", texture.image.extent.width, texture.image.extent.height, texture.image.extent.depth);
                            ImGui::Text("Format: %s", imageFormatToString(texture.image.format));
                            ImGui::Text("Mips: %d", texture.mipCount);
                            ImGui::Text("Bindless index: %d", textureHandle);

                            const auto isFree = backend.bindlessResources->freeIndices.contains(textureHandle);
                            if (isFree)
                            {
                                ImGui::SameLine();
                                ImGui::TextColored(ImVec4(255, 0, 0, 255), "Free");
                            }

                            ImGui::Separator();
                            const auto aspectRatio = static_cast<f32>(texture.image.extent.width) / static_cast<f32>(texture.image.extent.height);
                            ImVec2 size;
                            if (aspectRatio > 1.f)
                            {
                                size = ImVec2(512, 512.f / aspectRatio);
                            }
                            else
                            {
                               size = ImVec2(512.f * aspectRatio, 512);
                            }
                            ImGui::Image(*texture.imguiDescriptorSet, size);
                            ImGui::EndTooltip();
                        }
                    }
                };
            
            if (node.model && node.instance)
            {
                int materialHandle = static_cast<int>(scene.models.instances[*node.model][*node.instance].material.x);
                DefaultMaterial& material = scene.materials.materials[materialHandle];
                ImGui::Text("Material index:");
                ImGui::SameLine();
                ImGui::InputInt("##Material index", &materialHandle);
                scene.models.instances[*node.model][*node.instance].material.x = static_cast<f32>(materialHandle);

                ImGui::Text("Albedo: ");
                ImGui::SameLine();
                ImGui::InputInt("##Albedo index", (int*)&material.albedo);
                ImGui::SameLine();
                drawTexture(material.albedo);

                ImGui::Text("Normal: ");
                ImGui::SameLine();
                ImGui::InputInt("##Normal index", (int*)&material.normalTexture);
                ImGui::SameLine();
                drawTexture(material.normalTexture);

                ImGui::Text("Metallic-roughness: ");
                ImGui::SameLine();
                ImGui::InputInt("##Metallic-roughness index", (int*)&material.metallicRoughnessTexture);
                ImGui::SameLine();
                drawTexture(material.metallicRoughnessTexture);

                ImGui::Text("Bump: ");
                ImGui::SameLine();
                ImGui::InputInt("##Bump index", (int*)&material.bumpTexture);
                ImGui::SameLine();
                drawTexture(material.bumpTexture);

                ImGui::Text("Base color:");
                ImGui::SameLine();
                ImGui::ColorEdit4("##Base Color", &material.baseColor[0]);

                if (ImGui::CollapsingHeader("Features", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool lit = (material.features & DefaultMaterial::Features::LIT) != DefaultMaterial::Features::NONE;
                    if (ImGui::Checkbox("Lit", &lit))
                    {
                        material.features = material.features ^ DefaultMaterial::Features::LIT;
                    }
                    bool wireframe = (material.features & DefaultMaterial::Features::WIREFRAME) != DefaultMaterial::Features::NONE;
                    if (ImGui::Checkbox("Wireframe", &wireframe))
                    {
                        material.features = material.features ^ DefaultMaterial::Features::WIREFRAME;
                    }
                    bool normalMapping = (material.features & DefaultMaterial::Features::NORMAL_MAPPING) != DefaultMaterial::Features::NONE;
                    if (ImGui::Checkbox("Normal mapping", &normalMapping))
                    {
                        material.features = material.features ^ DefaultMaterial::Features::NORMAL_MAPPING;
                    }
                    bool parallax = (material.features & DefaultMaterial::Features::PARALLAX) != DefaultMaterial::Features::NONE;
                    if (ImGui::Checkbox("Parallax mapping", &parallax))
                    {
                        material.features = material.features ^ DefaultMaterial::Features::PARALLAX;
                    }
                }
            }
        }

        drawChildren(INSPECTOR);
    }
    ImGui::End();

    static bool resourcesOpen = true;
    if (ImGui::Begin(RESOURCES_CSTR, &graphicsOpen))
    {
        drawChildren(RESOURCES);
    }
    ImGui::End();

    if (ImGui::Begin(OUTPUT_CSTR, nullptr))
    {
        debugUI.outputFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        
        const auto padding = ImGui::GetCursorPos();
        const auto windowSize = ImGui::GetWindowContentRegionMax();
        const auto size = ImVec2(windowSize.x + padding.x, windowSize.y + padding.y);

        auto imguiPos = ImGui::GetWindowPos();
        auto imguiSize = size;

        drawChildren(OUTPUT);

        static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
        static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
        {
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | //ImGuiWindowFlags_AlwaysAutoResize |
                                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
            auto viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(imguiPos + padding, ImGuiCond_Always);
            bool open = true;
            if (ImGui::BeginChild("Manipulate", ImVec2(0, 0), ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, window_flags))
            {
                ImGuiStyle& style = ImGui::GetStyle();
                ImVec4* colors = style.Colors;
                auto color = colors[ImGuiCol_Button] + ImVec4(-0.3, -0.3, -0.3, 0.4);

                const auto translate = mCurrentGizmoOperation == ImGuizmo::TRANSLATE;
                if (translate)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, color);
                }
                if (ImGui::Button(ICON_FA_ARROWS_ALT))
                {
                    mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
                }
                ImGui::SameLine();
                if (translate)
                {
                    ImGui::PopStyleColor();
                }

                const auto rotate = mCurrentGizmoOperation == ImGuizmo::ROTATE;
                if (rotate)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, color);
                }
                if (ImGui::Button(ICON_FA_SYNC_ALT))
                {
                    mCurrentGizmoOperation = ImGuizmo::ROTATE;
                }
                ImGui::SameLine();
                if (rotate)
                {
                    ImGui::PopStyleColor();
                }

                const auto scale = mCurrentGizmoOperation == ImGuizmo::SCALE;
                if (scale)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, color);
                }
                if (ImGui::Button(ICON_FA_EXPAND_ALT))
                {
                    mCurrentGizmoOperation = ImGuizmo::SCALE;
                }
                ImGui::SameLine();
                if (scale)
                {
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndChild();
        }

        if (selectedHandle != SceneGraph::kRootHandle)
        {
            ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());
            ImGuizmo::SetRect(imguiPos.x, imguiPos.y, imguiSize.x, imguiSize.y);
            glm::mat4 view = scene.activeCamera->view();
            glm::mat4 proj = scene.activeCamera->proj();
            proj[1][1] *= -1.f;
            auto& node = scene.sceneGraph.nodes[selectedHandle];
            if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), mCurrentGizmoOperation, mCurrentGizmoMode,
                glm::value_ptr(node.globalTransform), NULL, NULL))
            {
                node.dirty = SceneGraph::NewNode::TransformDirtiness::LOCAL_DIRTY;
            }
        }
    }
    ImGui::End();

    drawChildren(GLOBAL);
}
