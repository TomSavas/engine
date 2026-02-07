#pragma once

auto sponzaScene(VulkanBackend& backend) -> Scene
{
    Scene scene = emptyScene(backend);
    
    scene.materials = initMaterials<DefaultMaterial>(backend, 8192);
    scene.models = initModels(backend, 4096 * 10000, 4096 * 10000, 8192);

    std::vector<glm::mat4> transforms;
    constexpr u32 instanceCount = 1;
    transforms.reserve(instanceCount * instanceCount);
    for (u32 z = 0; z < instanceCount; z++)
    {
        for (u32 x = 0; x < instanceCount; x++)
        {
            transforms.push_back(
                glm::translate(glm::scale(glm::mat4(1.f), glm::vec3(0.2f)), glm::vec3((x+1) * 30.f, 0.f, (z+1) * 20.f))
                // glm::translate(glm::scale(glm::mat4(1.f), glm::vec3(1.f)), glm::vec3(x * 30.f, 0.f, z * 20.f) * 5.f)
            );
        }
    }
    scene.addScene("../assets/Sponza/Sponza.gltf", transforms);
    // scene.addScene("../assets/intelsponza/sponza.gltf", transforms);
    // scene.addScene("../assets/Suzanne/Suzanne.gltf", transforms);

    return scene;
}

auto instancingTestScene(VulkanBackend& backend) -> Scene
{
    Scene scene = emptyScene(backend);

    scene.materials = initMaterials<DefaultMaterial>(backend, 8);
    constexpr auto w = BindlessResources::kWhite;
    std::array mats = {
        DefaultMaterial{w, w, w, w, {1.f, 1.f, 1.f, 1.f}},
        DefaultMaterial{w, w, w, w, {1.f, 0.f, 0.f, 1.f}},
        DefaultMaterial{w, w, w, w, {0.f, 1.f, 0.f, 1.f}},
        DefaultMaterial{w, w, w, w, {0.f, 0.f, 1.f, 1.f}},
        DefaultMaterial{w, w, w, w, {1.f, 0.f, 1.f, 1.f}},
    };
    const auto materials = addMaterials<DefaultMaterial>(backend, scene.materials, std::span(mats));

    scene.models = initModels(backend, 4096 * 1000, 4096 * 1000, 4096 * 1000);
    std::array models = {cubeModelData(), sphereModelData()};
    std::array modelDebugs = {
        Models::ModelDebug{"cube"},
        Models::ModelDebug{"sphere"},
    };
    const auto modelHandles = loadModels(backend, scene.models, models, modelDebugs);
    constexpr auto instanceCount = 4;
    std::array<Models::InstanceData, instanceCount*instanceCount> instances;
    for (u32 z = 0; z < instanceCount; z++)
    {
        for (u32 x = 0; x < instanceCount; x++)
        {
            instances[z * instanceCount + x] = Models::InstanceData {
                .transform = glm::scale(glm::translate(glm::mat4(1.f), glm::vec3(x * 2.5f, 0.f, z * 2.5f) * 0.05f), glm::vec3(0.05f)),
                .material = glm::vec4(materials[z % materials.size()]),
            };

            auto& m = mats[z % materials.size()];
            debugDrawCube(scene, glm::vec3(x * 2.5f, 0.f, z * 2.5f) * 0.05f, glm::vec3(0.05f), glm::vec3(m.baseColor[0], m.baseColor[1], m.baseColor[2]));
        }
    }
    // addInstances(backend, scene.models, modelHandles[0], instances);

    for (u32 z = 0; z < instanceCount; z++)
    {
        for (u32 x = 0; x < instanceCount; x++)
        {
            instances[z * instanceCount + x] = Models::InstanceData {
                .transform = glm::scale(glm::translate(glm::mat4(1.f), glm::vec3(x * 2.5f, 10.f, z * 2.5f) * 0.05f), glm::vec3(0.05f)),
                .material = glm::vec4(materials[(z+1) % materials.size()]),
            };

            auto& m = mats[z % materials.size()];
            debugDrawSphere(scene, glm::vec3(x * 2.5f, 10.f, z * 2.5f) * 0.05f, glm::vec3(0.05f), glm::vec3(m.baseColor[0], m.baseColor[1], m.baseColor[2]));
        }
    }
    // addInstances(backend, scene.models, modelHandles[1], instances);

    return scene;
}

/*
// Bad apple meme monitor simulation scene
auto iidx(u16 x, u16 y, u16 w, u16 h, u16 componentCount, u16 component) -> u32
{
    x = (x + w) % w;
    y = (y + h) % h;
    return componentCount * y * w + (componentCount * x + component);
}

constexpr u32 frameCount = 6572;
std::vector<u8*> frames;
ModelHandle box;
u16 frameIndex = 0;
constexpr auto instanceCountX = 72 * 3;
constexpr auto instanceCountZ = 128 * 3;
bool paused = true;

auto displayScene(VulkanBackend& backend) -> Scene
{
    // TODO: do a bad apple out of this lol
    Scene scene = emptyScene(backend);

    scene.materials = initMaterials<DefaultMaterial>(backend, 8);
    std::array mats = {
        DefaultMaterial{BindlessResources::kRed, BindlessResources::kWhite, BindlessResources::kWhite, BindlessResources::kWhite},
        DefaultMaterial{BindlessResources::kGreen, BindlessResources::kWhite, BindlessResources::kWhite, BindlessResources::kWhite},
        DefaultMaterial{BindlessResources::kBlue, BindlessResources::kWhite, BindlessResources::kWhite, BindlessResources::kWhite},
    };
    const auto materials = addMaterials<DefaultMaterial>(backend, scene.materials, std::span(mats));

    int width;
    int height;
    int components;
    for (u32 i = 1; i <= frameCount; ++i)
    {
        auto filename = std::format("../assets/bad_apple/frames_grayscale/out-{:04d}.jpg", i);
        u8* loadRes = stbi_load(filename.c_str(), &width, &height, &components, STBI_rgb);
        frames.push_back(loadRes);
    }

    scene.models = initModels(backend, 4096 * 1000, 4096 * 1000, instanceCountX * instanceCountZ);
    std::array cube = {cubeModelData()};
    box = loadModels(backend, scene.models, cube).back();
    std::vector<Models::InstanceData> instances;
    instances.resize(instanceCountX * instanceCountZ);
    for (u32 z = 0; z < instanceCountZ; z++)
    {
        for (u32 x = 0; x < instanceCountX; x++)
        {
            const u32 zChunkIndex = z / 3;
            const u32 xChunkIndex = x / 3;
            const bool newChunk = (z % 3) == 0;
            auto ind = iidx(zChunkIndex, height-xChunkIndex-1, width, height, components, 0);
            instances[z * instanceCountX + x] = Models::InstanceData {
                .transform = glm::scale(glm::translate(glm::mat4(1.f), glm::vec3(x * 1.75f + xChunkIndex * 1.f, 5.f, z * 1.75f + zChunkIndex * 0.5f) * 0.05f), glm::vec3(0.05f)),
                // .material = glm::vec4(materials[z % 3]),
                .material = glm::vec4(
                    (z % 3) == 0 ? frames[120][ind] : 0,
                    (z % 3) == 1 ? frames[120][ind+1] : 0,
                    (z % 3) == 2 ? frames[120][ind+2] : 0,
                    // 0, 0, 0,
                    255),
            };
        }
    }
    addInstances(backend, scene.models, box, instances);

    return scene;
}

void updateMonitor() -> void
{
    static auto debugKeyWasPressed = false;
    auto debugKeyPressed = glfwGetKey(backend.window, GLFW_KEY_F2) == GLFW_PRESS;
    if (debugKeyWasPressed && !debugKeyPressed)
    {
        paused = !paused;
    }
    debugKeyWasPressed = debugKeyPressed;

    if (!paused)
    {
        int width = instanceCountZ / 3;
        int height = instanceCountX / 3;
        int components = 3;

        // constexpr auto instanceCountX = 48 * 3;
        // constexpr auto instanceCountZ = 84 * 3;
        std::vector<Models::InstanceData> instances;
        instances.resize(instanceCountX * instanceCountZ);
        for (u32 z = 0; z < instanceCountZ; z++)
        {
            for (u32 x = 0; x < instanceCountX; x++)
            {
                const u32 zChunkIndex = z / 3;
                const u32 xChunkIndex = x / 3;
                const bool newChunk = (z % 3) == 0;
                auto ind = iidx(zChunkIndex, height-xChunkIndex-1, width, height, components, 0);
                instances[z * instanceCountX + x] = Models::InstanceData {
                    .transform = glm::scale(glm::translate(glm::mat4(1.f), glm::vec3(x * 1.75f + xChunkIndex * 1.f, 5.f, z * 1.75f + zChunkIndex * 0.5f) * 0.05f), glm::vec3(0.05f)),
                    .material = glm::vec4(
                        (z % 3) == 0 ? frames[frameIndex][ind] : 0,
                        (z % 3) == 1 ? frames[frameIndex][ind+1] : 0,
                        (z % 3) == 2 ? frames[frameIndex][ind+2] : 0,
                        // 0, 0, 0,
                        255),
                };
            }
        }
        scene.models.instances[box].clear();
        addInstances(backend, scene.models, box, instances);

        frameIndex = (frameIndex + 1) % frameCount;
    }
}
*/
