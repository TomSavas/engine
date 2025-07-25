#include "scene.h"

#include <algorithm>
#include <atomic>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <print>
#include <random>

#include "GLFW/glfw3.h"
#include "imageProcessing/displacement.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/inits.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "tracy/Tracy.hpp"

glm::vec3 right(glm::mat4 mat) { return mat * glm::vec4(1.f, 0.f, 0.f, 0.f); }

glm::vec3 up(glm::mat4 mat) { return mat * glm::vec4(0.f, 1.f, 0.f, 0.f); }

glm::vec3 forward(glm::mat4 mat) { return mat * glm::vec4(0.f, 0.f, -1.f, 0.f); }

static std::atomic<double> yOffsetAtomic;
void scrollCallback(GLFWwindow* window, double xoffset, double yOffset) { yOffsetAtomic.store(yOffset); }

void updateFreeCamera(float dt, GLFWwindow* window, Camera& camera)
{
    ZoneScoped;

    static bool scrollCallbackSet = false;
    if (!scrollCallbackSet)
    {
        glfwSetScrollCallback(window, scrollCallback);
        scrollCallbackSet = true;
    }

    float scrollWheelChange = yOffsetAtomic.exchange(0.0);
    camera.moveSpeed = std::max(0.f, camera.moveSpeed + scrollWheelChange);

    static glm::dvec2 lastMousePos = glm::vec2(-1.f - 1.f);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        static double radToVertical = .0;
        static double radToHorizon = .0;

        if (lastMousePos.x == -1.f)
        {
            glfwGetCursorPos(window, &lastMousePos.x, &lastMousePos.y);
        }

        glm::dvec2 mousePos;
        glfwGetCursorPos(window, &mousePos.x, &mousePos.y);
        glm::dvec2 mousePosDif = mousePos - lastMousePos;
        lastMousePos = mousePos;

        radToVertical += mousePosDif.x * camera.rotationSpeed / camera.aspectRatio;
        while (radToVertical > glm::pi<float>() * 2)
        {
            radToVertical -= glm::pi<float>() * 2;
        }
        while (radToVertical < -glm::pi<float>() * 2)
        {
            radToVertical += glm::pi<float>() * 2;
        }

        radToHorizon -= mousePosDif.y * camera.rotationSpeed;
        radToHorizon = std::min(std::max(radToHorizon, -glm::pi<double>() / 2 + 0.01), glm::pi<double>() / 2 - 0.01);

        camera.rotation = glm::eulerAngleYX(-radToVertical, radToHorizon);
    }
    else
    {
        glfwGetCursorPos(window, &lastMousePos.x, &lastMousePos.y);
    }

    glm::vec3 dir(0.f, 0.f, 0.f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        dir += forward(camera.rotation);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        dir -= forward(camera.rotation);
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        dir += right(camera.rotation);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        dir -= right(camera.rotation);
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        dir += up(camera.rotation);
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        dir -= up(camera.rotation);
    }

    camera.position += dir * camera.moveSpeed * dt;
}

void updateLights(float dt, std::vector<PointLight>& pointLights)
{
    static double time = 0.f;
    //static glm::vec3 initial = pointLights[0].pos;
    //static float dist = glm::length(glm::vec2(initial));
    time += dt;

    for (auto& light : pointLights)
    {
        float dist = glm::length(glm::vec2(light.pos.x, light.pos.z));
        float angle = atan2(light.pos.x, light.pos.z);

        light.pos.x = sin(angle + dt * 4.f) * dist;
        light.pos.z = cos(angle + dt * 4.f) * dist;
    }
}

void Scene::update(float dt, float currentTimeMs, GLFWwindow* window)
{
    static bool released = true;

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && released)
    {
        released = false;
        activeCamera = (activeCamera == &mainCamera) ? &debugCamera : &mainCamera;

        bool isMain = (bool)(activeCamera == &mainCamera);
        bool isDebug = (bool)(activeCamera == &debugCamera);
        std::println("active: {:x}, main: {:x}({}), debug: {:x}({})", (uint64_t)activeCamera, (uint64_t)&mainCamera,
            isMain, (uint64_t)&debugCamera, isDebug);
    }

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE)
    {
        released = true;
    }

    updateFreeCamera(dt, window, *activeCamera);
    updateLights(dt, pointLights);

    //glm::mat4 invProj = glm::inverse(activeCamera->proj());
    //glm::vec4 a = invProj * glm::vec4(0.f, 0.f, -1.f, 1.f);
    //a /= a.w;
    //std::println("-1.0 in VS: {} {} {} {}", a.x, a.y, a.z, a.w);
    //a = invProj * glm::vec4(0.f, 0.f, 0.f, 1.f);
    //a /= a.w;
    //std::println(" 0.0 in VS: {} {} {} {}", a.x, a.y, a.z, a.w);
    //a = invProj * glm::vec4(0.f, 0.f, 0.5f, 1.f);
    //a /= a.w;
    //std::println(" 0.5 in VS: {} {} {} {}", a.x, a.y, a.z, a.w);
    //a = invProj * glm::vec4(0.f, 0.f, 1.f, 1.f);
    //a /= a.w;
    //std::println(" 1.0 in VS: {} {} {} {}", a.x, a.y, a.z, a.w);

    //a = activeCamera->view() * glm::vec4(0.f, 0.f, 0.f, 1.f);
    //a /= a.w;
    //std::println("!0.0 in VS: {} {} {} {}", a.x, a.y, a.z, a.w);
}

void Scene::load(const char* path)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    if (!loader.LoadASCIIFromFile(&model, &err, &warn, path))
    {
        std::println("{}", err);
        std::println("{}", warn);
    }
    else
    {
        std::println("Successfully loaded {}", path);
    }

    addMeshes(model);
    createBuffers();
}

void Scene::addMeshes(tinygltf::Model& model, glm::vec3 offset)
{
    // Matches Vertex definition
    const char* position = "POSITION";
    const std::pair<const char*, int> attributes[] = {
        {position, 4},
        {"TEXCOORD_0", 4},
        {"NORMAL", 4},
        {"TANGENT", 4},
    };

    for (tinygltf::Mesh& mesh : model.meshes)
    {
        int primitiveCount = 0;
        for (tinygltf::Primitive& primitive : mesh.primitives)
        {
            Mesh& m = meshes.emplace_back();
            m.debugName = std::format("{}_{}", mesh.name, primitiveCount++);
            // std::println("{}", m.debugName);
            m.indexOffset = indices.size();
            m.vertexOffset = vertexData.size();
            // m.materialIndex = primitive.material;

            m.aabbMin = glm::vec3(0.f);
            m.aabbMax = glm::vec3(0.f);

            int vertexAttributeOffset = 0;
            for (const auto& [attribute, attributeCount] : attributes)
            {
                if (primitive.attributes.find(attribute) == primitive.attributes.end())
                {
                    continue;
                }

                tinygltf::Accessor accessor = model.accessors[primitive.attributes[std::string(attribute)]];
                tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                float* data = reinterpret_cast<float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);

                for (int i = 0; i < accessor.count; i++)
                {
                    int vertexIndex = m.vertexOffset + i;
                    if (vertexData.size() <= vertexIndex)
                    {
                        vertexData.emplace_back();
                    }
                    Vertex& vertex = vertexData[vertexIndex];

                    const int componentCount = tinygltf::GetNumComponentsInType(accessor.type);
                    for (int j = 0; j < componentCount; j++)
                    {
                        vertex.raw[vertexAttributeOffset + j] = data[i * componentCount + j];
                    }

                    if (attribute == position)
                    {
                        vertex.pos[0] += offset.x;
                        vertex.pos[1] += offset.y;
                        vertex.pos[2] += offset.z;

                        m.aabbMin.x = std::min(m.aabbMin.x, vertex.pos[0]);
                        m.aabbMin.y = std::min(m.aabbMin.y, vertex.pos[1]);
                        m.aabbMin.z = std::min(m.aabbMin.z, vertex.pos[2]);

                        m.aabbMax.x = std::max(m.aabbMax.x, vertex.pos[0]);
                        m.aabbMax.y = std::max(m.aabbMax.y, vertex.pos[1]);
                        m.aabbMax.z = std::max(m.aabbMax.z, vertex.pos[2]);
                    }
                }
                // std::println("{} {} {}", vertexData.back().pos[0], vertexData.back().pos[1],
                // vertexData.back().pos[2]);
                vertexAttributeOffset += attributeCount;
            }

            // Update scene AABB
            aabbMin.x = std::min(aabbMin.x, m.aabbMin.x);
            aabbMin.y = std::min(aabbMin.y, m.aabbMin.y);
            aabbMin.z = std::min(aabbMin.z, m.aabbMin.z);
            aabbMax.x = std::max(aabbMax.x, m.aabbMax.x);
            aabbMax.y = std::max(aabbMax.y, m.aabbMax.y);
            aabbMax.z = std::max(aabbMax.z, m.aabbMax.z);

            tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];
            tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
            const unsigned short* indexData = reinterpret_cast<unsigned short*>(
                &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
            for (size_t i = 0; i < indexAccessor.count; i++)
            {
                indices.push_back(indexData[i] + m.vertexOffset);
            }

            m.vertexCount = vertexData.size() - m.vertexOffset;
            m.indexCount = indices.size() - m.indexOffset;

            // Material
            if (primitive.material == -1)
            {
                continue;
            }
            tinygltf::Material& material = model.materials[primitive.material];

            // TODO: Base color factor
            tinygltf::PbrMetallicRoughness& pbr = material.pbrMetallicRoughness;
            tinygltf::TextureInfo& albedoTextureInfo = pbr.baseColorTexture;
            if (albedoTextureInfo.index != -1)
            {
                // TODO: metallicRoughnessTexture
                tinygltf::Texture& albedo = model.textures[albedoTextureInfo.index];
                // TODO: don't ignore sampler
                // TODO: don't ignore texCoord index
                tinygltf::Image& albedoImg = model.images[albedo.source];
                m.albedoTexture = images.size();
                images.push_back(albedoImg);

                // TODO: remove above
                auto maybeTexture = backend.textures->loadRaw(albedoImg.image.data(), albedoImg.image.size(),
                    albedoImg.width, albedoImg.height, true, false, albedoImg.uri);
                bindlessImages.push_back(backend.bindlessResources->addTexture(std::get<0>(*maybeTexture)));
                m.albedoTexture = bindlessImages.back();
            }

            tinygltf::NormalTextureInfo& normalTextureInfo = material.normalTexture;
            if (normalTextureInfo.index != -1)
            {
                tinygltf::Texture& normal = model.textures[normalTextureInfo.index];
                // TODO: don't ignore texCoord index
                // TODO: don't ignore sampler
                tinygltf::Image& normalImg = model.images[normal.source];
                m.normalTexture = images.size();
                images.push_back(normalImg);

                // std::println("Normal texture components {} with {}b per component", normalImg.component, normalImg.bits);

                // TODO: remove above
                auto maybeTexture = backend.textures->loadRaw(normalImg.image.data(), normalImg.image.size(),
                    normalImg.width, normalImg.height, true, true, normalImg.uri);
                bindlessImages.push_back(backend.bindlessResources->addTexture(std::get<0>(*maybeTexture)));
                m.normalTexture = bindlessImages.back();

                std::string bumpFilename = "generatedBump_" + normalImg.uri + ".png";
                // TODO: allow specifying format

                int bumpWidth = normalImg.width;
                int bumpHeight = normalImg.height;
                int components;
                uint8_t* loadRes = stbi_load(bumpFilename.c_str(), &bumpWidth, &bumpHeight, &components, STBI_rgb_alpha);
                if (loadRes == nullptr)
                {
                    bumpWidth = normalImg.width;
                    bumpHeight = normalImg.height;

                    std::print("Generating bump map: {}... ", bumpFilename);
                    std::vector<uint8_t> bumpMapData = tangentNormalMapToBumpMap(normalImg.image.data(), normalImg.width,
                        normalImg.height);
                    std::println("done");

                    stbi_write_png(bumpFilename.c_str(), bumpHeight, bumpWidth, 4, bumpMapData.data(), 0);
                    maybeTexture = backend.textures->loadRaw(bumpMapData.data(), bumpMapData.size(), bumpWidth,
                        bumpHeight, true, true, bumpFilename);
                }
                else
                {
                    maybeTexture = backend.textures->loadRaw(loadRes, bumpWidth * bumpHeight * 4 * 1, bumpWidth,
                        bumpHeight, true, true, bumpFilename);
                }

                bindlessImages.push_back(backend.bindlessResources->addTexture(std::get<0>(*maybeTexture)));
                m.bumpTexture = bindlessImages.back();
            }
        }
    }
}

void Scene::createBuffers()
{
    const uint32_t vertexBufferSize = vertexData.size() * sizeof(decltype(vertexData)::value_type);
    const uint32_t indexBufferSize = indices.size() * sizeof(decltype(indices)::value_type);

    std::println("Vert count: {}, element size: {}, total size: {}", vertexData.size(),
        sizeof(decltype(vertexData)::value_type), vertexBufferSize);
    std::println("Index count: {}, element size: {}, total size: {}", indices.size(),
        sizeof(decltype(indices)::value_type), indexBufferSize);

    auto info = vkutil::init::bufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    vertexBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    info = vkutil::init::bufferCreateInfo(
        indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    indexBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_GPU_ONLY, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    backend.copyBufferWithStaging(vertexData.data(), vertexBufferSize, vertexBuffer.buffer);
    backend.copyBufferWithStaging(indices.data(), indexBufferSize, indexBuffer.buffer);

    struct ModelData
    {
        glm::vec4 textures;
        glm::mat4 model;
    };
    std::vector<ModelData> modelData;
    modelData.reserve(meshes.size());
    for (auto& mesh : meshes)
    {
        ModelData data;
        // data.albedoTex = mesh.albedoTexture;
        // data.normalTex = mesh.normalTexture;
        data.textures = glm::vec4(mesh.albedoTexture, mesh.normalTexture, mesh.bumpTexture, 0.f);
        data.model = glm::mat4(1.f);
        modelData.push_back(data);
    }

    const uint32_t perModelBufferSize = modelData.size() * sizeof(decltype(modelData)::value_type);
    info = vkutil::init::bufferCreateInfo(perModelBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    perModelBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_GPU_ONLY,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    backend.copyBufferWithStaging(modelData.data(), modelData.size() * sizeof(ModelData), perModelBuffer.buffer);

    info = vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * meshes.size(),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    indirectCommands = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    std::vector<VkDrawIndexedIndirectCommand> cmds;
    cmds.reserve(meshes.size());
    for (uint32_t i = 0; i < meshes.size(); ++i)
    {
        auto& mesh = meshes[i];
        VkDrawIndexedIndirectCommand command = {.indexCount = static_cast<uint32_t>(mesh.indexCount),
            .instanceCount = 1,
            .firstIndex = static_cast<uint32_t>(mesh.indexOffset),
            .vertexOffset = 0,
            .firstInstance = i};
        cmds.push_back(command);
    }
    backend.copyBufferWithStaging(
        cmds.data(), sizeof(VkDrawIndexedIndirectCommand) * cmds.size(), indirectCommands.buffer);
}

result::result<Scene, assetError> loadScene(VulkanBackend& backend, std::string name, std::string path,
    uint lightCount)
{
    Scene scene = Scene(name, backend);

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    std::println("Loading {}", path);
    if (!loader.LoadASCIIFromFile(&model, &err, &warn, path))
    {
        std::println("{}", err);
        std::println("{}", warn);
        return result::fail(assetError{});
    }

    std::println("Successfully loaded {}", path);
    scene.addMeshes(model);
    scene.createBuffers();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> uniformDistribution(0, 1);
    scene.pointLights.reserve(lightCount);

    scene.pointLights.push_back(PointLight {
        //.pos = glm::vec4(0, 30, 0, 1.f),
        //.color = glm::vec4(1.f, 0.6f, 0.2f, 1.f),
        //.range = glm::vec4(50.f),
        .pos = glm::vec4(22.7, 98.65, 115.17, 1.f),
        .color = glm::vec4(1.f, 0.95f, 0.8f, 1.f),
        .range = glm::vec4(150.f),
    });

    glm::vec3 exclusionAabb[2] = {
        glm::vec3(-900.f, 80.f, -200.f),
        glm::vec3(750.f, 1300.f, 120.f)
    };

    auto diff = scene.aabbMax - scene.aabbMin;
    auto offsetMin = scene.aabbMin + diff * 0.15f;
    offsetMin.x -= diff.y * 0.05f;
    offsetMin.y -= diff.y * 0.1f;
    for (int i = 0; i < lightCount - 1; ++i)
    {
        auto insideExclusion = [&](glm::vec3 pos)
        {
            return exclusionAabb[0].x < pos.x && pos.x < exclusionAabb[1].x &&
                exclusionAabb[0].y < pos.y && pos.y < exclusionAabb[1].y &&
                exclusionAabb[0].z < pos.z && pos.z < exclusionAabb[1].z;
        };

        glm::vec3 pos;
        do
        {
            //std::println("Regening {} {} {}", pos.x, pos.y, pos.z);
            pos = glm::vec3(
                uniformDistribution(gen) * 0.75 * diff.x + offsetMin.x,
                uniformDistribution(gen) * 0.6 * diff.y + offsetMin.y,
                uniformDistribution(gen) * 0.65 * diff.z + offsetMin.z
            );
        } while (insideExclusion(pos));
        //std::println("\t OK {} {} {}", pos.x, pos.y, pos.z);

        scene.pointLights.push_back(PointLight{
            .pos = glm::vec4(pos, 1.f),
            .color = glm::vec4(
                uniformDistribution(gen) * 0.6 + 0.4,
                uniformDistribution(gen) * 0.6 + 0.4,
                uniformDistribution(gen) * 0.6 + 0.4,
                1.f
            ),
            .range = glm::vec4(uniformDistribution(gen) * 40 + 20), // [20; 200]
        });
    }

    return scene;
}

Scene emptyScene(VulkanBackend& backend) { return Scene("empty", backend); }
