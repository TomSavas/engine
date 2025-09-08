#include "scene.h"

#include <algorithm>
#include <atomic>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <print>
#include <random>

#include "GLFW/glfw3.h"
#include "debugUI.h"
#include "glm/gtc/type_ptr.hpp"
#include "imageProcessing/displacement.h"
#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/inits.h"
#include "sceneGraph.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "tracy/Tracy.hpp"

glm::vec3 right(glm::mat4 mat) { return mat * glm::vec4(1.f, 0.f, 0.f, 0.f); }

glm::vec3 up(glm::mat4 mat) { return mat * glm::vec4(0.f, 1.f, 0.f, 0.f); }

glm::vec3 forward(glm::mat4 mat) { return mat * glm::vec4(0.f, 0.f, -1.f, 0.f); }

static std::atomic<f64> yOffsetAtomic;
void scrollCallback(GLFWwindow* window, f64 xoffset, f64 yOffset) { yOffsetAtomic.store(yOffset); }

void updateFreeCamera(f32 dt, GLFWwindow* window, Camera& camera)
{
    ZoneScoped;

    static bool scrollCallbackSet = false;
    if (!scrollCallbackSet)
    {
        glfwSetScrollCallback(window, scrollCallback);
        scrollCallbackSet = true;
    }

    f32 scrollWheelChange = yOffsetAtomic.exchange(0.0);
    camera.moveSpeed = std::max(0.f, camera.moveSpeed + scrollWheelChange);

    static glm::dvec2 lastMousePos = glm::vec2(-1.f - 1.f);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        static f64 radToVertical = .0;
        static f64 radToHorizon = .0;

        if (lastMousePos.x == -1.f)
        {
            glfwGetCursorPos(window, &lastMousePos.x, &lastMousePos.y);
        }

        glm::dvec2 mousePos;
        glfwGetCursorPos(window, &mousePos.x, &mousePos.y);
        glm::dvec2 mousePosDif = mousePos - lastMousePos;
        lastMousePos = mousePos;

        radToVertical += mousePosDif.x * camera.rotationSpeed / camera.aspectRatio;
        while (radToVertical > glm::pi<f32>() * 2)
        {
            radToVertical -= glm::pi<f32>() * 2;
        }
        while (radToVertical < -glm::pi<f32>() * 2)
        {
            radToVertical += glm::pi<f32>() * 2;
        }

        radToHorizon -= mousePosDif.y * camera.rotationSpeed;
        radToHorizon = std::min(std::max(radToHorizon, -glm::pi<f64>() / 2 + 0.01), glm::pi<f64>() / 2 - 0.01);

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

void updateLights(f32 dt, std::vector<PointLight>& pointLights)
{
    static f64 time = 0.f;
    //static glm::vec3 initial = pointLights[0].pos;
    //static f32 dist = glm::length(glm::vec2(initial));
    //time += dt * 0.2f;
    time += dt;

    // Ignoring y component
    glm::vec2 ellipseFocals[] = {
        glm::vec2(490.f, 0.f) * 0.01f,
        glm::vec2(-612.f, 0.f) * 0.01f,
    };
    f32 focalDist = glm::distance(ellipseFocals[0], ellipseFocals[1]);

    for (auto& light : pointLights)
    {
        // //dists[0] = glm::distance(ellipseFocals[0], glm::vec2(pointLights[0].pos.x, pointLights[0].pos.z));
        // f32 dists[] = {
        //     glm::distance(ellipseFocals[0], glm::vec2(pointLights[0].pos.x, pointLights[0].pos.z)),
        //     glm::distance(ellipseFocals[1], glm::vec2(pointLights[0].pos.x, pointLights[0].pos.z)),
        // };

        // f32 k = dists[0] + dists[1];
        // f32 a = sqrt(k * k - focalDist * focalDist) / 2.f;
        // //f32 b = k - dists[1] - glm::length(ellipseFocals[0]);
        // //f32 b = k - dists[1] - focalDist / 2.f;
        // //f32 b = k - glm::length(ellipseFocals[0]) - glm::length(ellspseFocals[1]) / 2.f;
        // //f32 b = (k - focalDist - glm::length(ellipseFocals[0]) * 2.f) / 2.f;
        // f32 b = (k - 2 * focalDist) / 2.f;

        // //f32 dist = glm::length(glm::vec2(light.pos.x, light.pos.z));
        // //f32 angle = atan2(ellipseFocals[1].x - light.pos.x, ellipseFocals[1].y - light.pos.z);

        // //light.pos.x = sin(angle + dt * 4.f) * a;
        // //light.pos.z = cos(angle + dt * 4.f) * b;

        // light.pos.x = cos(time) * b;
        // light.pos.z = sin(time) * a;

        // std::println("a={} b={} k={}", a, b, k);
        // //std::println("{} {}", light.pos.x, light.pos.z);


        f32 dist = glm::length(glm::vec2(light.pos.x, light.pos.z));
        f32 angle = atan2(light.pos.x, light.pos.z);

        light.pos.x = sin(angle + dt * 0.2) * dist;
        light.pos.z = cos(angle + dt * 0.2) * dist;
    }
}

struct ModelData
{
    glm::vec4 textures;
    glm::vec4 selected;
    glm::mat4 model;
};

auto gatherModelData(Scene& scene) -> std::vector<ModelData>
{
    std::vector<ModelData> modelData;
    modelData.reserve(scene.meshCount);
    for (auto& mesh : scene.meshes)
    {
        for (auto& instance : mesh.second.instances)
        {
            ModelData data;
            data.textures = glm::vec4(mesh.second.albedoTexture, mesh.second.normalTexture, mesh.second.bumpTexture, mesh.second.metallicRoughnessTexture);
            data.model = instance.modelTransform;
            data.selected = glm::vec4(instance.selected ? 1.f : 0.f);
            modelData.push_back(data);
        }
    }

    return modelData;
}


void Scene::update(f32 dt, f32 currentTimeMs, GLFWwindow* window)
{
    // Selection logic
    static std::string selectedMesh;
    static int selectedInstance = 0;

    if (!selectedMesh.empty())
    {
        meshes[selectedMesh].instances[selectedInstance].selected = false;
    }

    if (!debugUI.selectedNode.empty())
    {
        auto meshDelim = debugUI.selectedNode.rfind('_');
        auto instanceDelim = debugUI.selectedNode.rfind(':');

        if (meshDelim != std::string::npos && instanceDelim != std::string::npos)
        {
            selectedMesh = debugUI.selectedNode.substr(0, meshDelim);
            auto selectedInstanceStr = debugUI.selectedNode.substr(instanceDelim + 1, 1);
            selectedInstance = std::stoi(selectedInstanceStr);

            // std::println("Selected mesh: {}, selected instance: {}", selectedMesh, selectedInstance);

            meshes[selectedMesh].instances[selectedInstance].selected = true;
        }
    }

    updateSceneGraphTransforms(sceneGraph);
    // TODO: move to a render pass
    {
        auto modelData = gatherModelData(*this);
        backend.copyBufferWithStaging(modelData.data(), modelData.size() * sizeof(ModelData), perModelBuffer.buffer);
    }

    static bool released = true;

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && released)
    {
        released = false;
        activeCamera = (activeCamera == &mainCamera) ? &debugCamera : &mainCamera;

        bool isMain = (bool)(activeCamera == &mainCamera);
        bool isDebug = (bool)(activeCamera == &debugCamera);
        std::println("active: {:x}, main: {:x}({}), debug: {:x}({})", (u64)activeCamera, (u64)&mainCamera,
            isMain, (u64)&debugCamera, isDebug);
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

    addModel(model);
    createBuffers();
}

void Scene::addModel(tinygltf::Model& model, glm::mat4 transform)
{
    auto* sceneGraphNode = new SceneGraph::Node("model", glm::mat4(1.f), glm::mat4(1.f), 0, sceneGraph.root);
    sceneGraph.root->children.push_back(sceneGraphNode);

    for (auto& node : model.nodes)
    {
        addNodes(model, node, transform, *sceneGraphNode);
    }
}

void Scene::addNodes(tinygltf::Model& model, tinygltf::Node& node, glm::mat4 transform, SceneGraph::Node& parent)
{
    glm::mat4 localTransform = glm::mat4(1.0f);
    if (node.matrix.empty())
    {
        localTransform = (node.translation.empty() ? glm::mat4(1.f) : glm::translate(glm::vec3(node.translation[0], node.translation[1], node.translation[2]))) *
            (node.rotation.empty() ? glm::mat4(1.f) : glm::toMat4(glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]))) *
            (node.scale.empty() ? glm::mat4(1.f) : glm::scale(glm::vec3(node.scale[0], node.scale[1], node.scale[2])));
    }
    else
    {
        localTransform = glm::mat4(glm::make_mat4(node.matrix.data()));
    }
    transform = transform * localTransform;

    static int a = 0;
    auto* sceneGraphNode = new SceneGraph::Node(node.name, localTransform, transform, 0, &parent);
    parent.children.push_back(sceneGraphNode);

    if (node.name.empty())
    {
        sceneGraphNode->name = std::format("gltf_node_{}", a++);
    }

    if (node.mesh != -1)
    {
        addMesh(model, model.meshes[node.mesh], transform, *sceneGraphNode);
        //sceneGraphNode->name = model.meshes[node.mesh].name;
    }

    for (auto& child : node.children)
    {
        addNodes(model, model.nodes[child], transform, *sceneGraphNode);
    }
}

void Scene::addMesh(tinygltf::Model& model, tinygltf::Mesh& mesh, glm::mat4 transform, SceneGraph::Node& parent)
{
    // Matches Vertex definition
    const char* position = "POSITION";
    const std::pair<const char*, i32> attributes[] = {
        {position, 4},
        {"TEXCOORD_0", 4},
        {"NORMAL", 4},
        {"TANGENT", 4},
    };

    // TEMP: avoid decals in intel sponza for now
    if (mesh.name.contains("decal"))
    {
        return;
    }

    i32 primitiveCount = 0;
    for (tinygltf::Primitive& primitive : mesh.primitives)
    {
        meshCount++;
        auto debugName = std::format("{}_{}", mesh.name.empty() ? "unnamed" : mesh.name, primitiveCount++);

        if (auto it = meshes.find(debugName); it != meshes.end())
        {
            Instance instance = {
                .modelTransform = transform,
                .aabbMin = transform * glm::vec4(it->second.aabbMin, 1.f),
                .aabbMax = transform * glm::vec4(it->second.aabbMax, 1.f),
                .selected = false,
            };
            it->second.instances.push_back(instance);

            auto* sceneGraphNode = new SceneGraph::Node(std::format("{}_inst:{}", debugName, it->second.instances.size()-1), glm::mat4(1.f), transform, 0, &parent);
            sceneGraphNode->instance = &it->second.instances.back(); // This will crash 100%
            parent.children.push_back(sceneGraphNode);

            aabbMin.x = std::min(aabbMin.x, instance.aabbMin.x);
            aabbMin.y = std::min(aabbMin.y, instance.aabbMin.y);
            aabbMin.z = std::min(aabbMin.z, instance.aabbMin.z);
            aabbMax.x = std::max(aabbMax.x, instance.aabbMax.x);
            aabbMax.y = std::max(aabbMax.y, instance.aabbMax.y);
            aabbMax.z = std::max(aabbMax.z, instance.aabbMax.z);
            return;
        }
        Mesh& m = meshes.insert({debugName, Mesh{}}).first->second;

        //m.debugName = std::format("{}_{}", mesh.name, primitiveCount++);
        m.debugName = debugName;
        m.indexOffset = indices.size();
        m.vertexOffset = vertexData.size();
        // m.materialIndex = primitive.material;

        m.aabbMin = glm::vec3(0.f);
        m.aabbMax = glm::vec3(0.f);

        i32 vertexAttributeOffset = 0;
        for (const auto& [attribute, attributeCount] : attributes)
        {
            //std::println("\tAttribute {}...", attribute);
            if (primitive.attributes.find(attribute) == primitive.attributes.end())
            {
                continue;
            }

            tinygltf::Accessor accessor = model.accessors[primitive.attributes[std::string(attribute)]];
            tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            f32* data = reinterpret_cast<f32*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);

            for (i32 i = 0; i < accessor.count; i++)
            {
                i32 vertexIndex = m.vertexOffset + i;
                if (vertexData.size() <= vertexIndex)
                {
                    vertexData.emplace_back();
                }
                Vertex& vertex = vertexData[vertexIndex];

                const i32 componentCount = tinygltf::GetNumComponentsInType(accessor.type);
                for (i32 j = 0; j < componentCount; j++)
                {
                    vertex.raw[vertexAttributeOffset + j] = data[i * componentCount + j];
                }

                if (attribute == position)
                {
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

        Instance instance = {
            .modelTransform = transform,
            .aabbMin = transform * glm::vec4(m.aabbMin, 1.f),
            .aabbMax = transform * glm::vec4(m.aabbMax, 1.f),
            .selected = false,
        };
        m.instances.push_back(instance);

        auto* sceneGraphNode = new SceneGraph::Node(std::format("{}_inst:0", debugName), glm::mat4(1.f), transform, 0, &parent);
        sceneGraphNode->instance = &m.instances.back(); // This will crash 100%
        parent.children.push_back(sceneGraphNode);

        // Update scene AABB
        aabbMin.x = std::min(aabbMin.x, instance.aabbMin.x);
        aabbMin.y = std::min(aabbMin.y, instance.aabbMin.y);
        aabbMin.z = std::min(aabbMin.z, instance.aabbMin.z);
        aabbMax.x = std::max(aabbMax.x, instance.aabbMax.x);
        aabbMax.y = std::max(aabbMax.y, instance.aabbMax.y);
        aabbMax.z = std::max(aabbMax.z, instance.aabbMax.z);

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

            // TODO: remove above
            auto maybeTexture = backend.textures->loadRaw(albedoImg.image.data(), albedoImg.image.size(),
                albedoImg.width, albedoImg.height, true, true, albedoImg.uri);
            bindlessImages.push_back(backend.bindlessResources->addTexture(std::get<0>(*maybeTexture)));
            m.albedoTexture = bindlessImages.back();
        }

        //tinygltf::TextureInfo& metallicRoughnessTextureInfo = pbr.metallicRoughnessTexture;
        //if (metallicRoughnessTextureInfo.index != -1)
        //{
        //    // TODO: metallicRoughnessTexture
        //    tinygltf::Texture& metallicRoughness = model.textures[metallicRoughnessTextureInfo.index];
        //    // TODO: don't ignore sampler
        //    // TODO: don't ignore texCoord index
        //    tinygltf::Image& metallicRoughnessImg = model.images[metallicRoughness.source];

        //    // TODO: remove above
        //    auto maybeTexture = backend.textures->loadRaw(metallicRoughnessImg.image.data(), metallicRoughnessImg.image.size(),
        //        metallicRoughnessImg.width, metallicRoughnessImg.height, true, true, metallicRoughnessImg.uri);
        //    bindlessImages.push_back(backend.bindlessResources->addTexture(std::get<0>(*maybeTexture)));
        //    m.metallicRoughnessTexture = bindlessImages.back();
        //}
        m.metallicRoughnessTexture = BindlessResources::kWhite;

        tinygltf::NormalTextureInfo& normalTextureInfo = material.normalTexture;
        if (normalTextureInfo.index != -1)
        {
            tinygltf::Texture& normal = model.textures[normalTextureInfo.index];
            // TODO: don't ignore texCoord index
            // TODO: don't ignore sampler
            tinygltf::Image& normalImg = model.images[normal.source];

            // TODO: remove above
            auto maybeTexture = backend.textures->loadRaw(normalImg.image.data(), normalImg.image.size(),
                normalImg.width, normalImg.height, true, true, normalImg.uri);
            bindlessImages.push_back(backend.bindlessResources->addTexture(std::get<0>(*maybeTexture)));
            m.normalTexture = bindlessImages.back();

            std::string bumpFilename = "generatedBump_" + normalImg.uri + ".png";
            // TODO: allow specifying format

            i32 bumpWidth = normalImg.width;
            i32 bumpHeight = normalImg.height;
            i32 components;
            u8* loadRes = stbi_load(bumpFilename.c_str(), &bumpWidth, &bumpHeight, &components, STBI_rgb_alpha);
            if (loadRes == nullptr)
            {
                //bumpWidth = normalImg.width;
                //bumpHeight = normalImg.height;

                //std::println("Generating bump map: {}... ", bumpFilename);
                //std::vector<u8> bumpMapData = tangentNormalMapToBumpMap(normalImg.image.data(), normalImg.width,
                //    normalImg.height);

                //stbi_write_png(bumpFilename.c_str(), bumpHeight, bumpWidth, 4, bumpMapData.data(), 0);
                //maybeTexture = backend.textures->loadRaw(bumpMapData.data(), bumpMapData.size(), bumpWidth,
                //    bumpHeight, true, true, bumpFilename);
                bumpFilename = "empty_bump";
                maybeTexture = std::tuple<Texture, std::string>(whiteTexture(backend, 2.f), bumpFilename);
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

void Scene::createBuffers()
{
    const u32 vertexBufferSize = vertexData.size() * sizeof(decltype(vertexData)::value_type);
    const u32 indexBufferSize = indices.size() * sizeof(decltype(indices)::value_type);

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

    auto modelData = gatherModelData(*this);
    const u32 perModelBufferSize = modelData.size() * sizeof(decltype(modelData)::value_type);
    info = vkutil::init::bufferCreateInfo(perModelBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    perModelBuffer = backend.allocateBuffer(info, VMA_MEMORY_USAGE_GPU_ONLY,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    backend.copyBufferWithStaging(modelData.data(), modelData.size() * sizeof(ModelData), perModelBuffer.buffer);

    info = vkutil::init::bufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand) * modelData.size(),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    indirectCommands = backend.allocateBuffer(info, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // TODO: do actual instancing
    std::vector<VkDrawIndexedIndirectCommand> cmds;
    cmds.reserve(modelData.size());
    u32 i = 0;
    for (auto& mesh : meshes)
    {
        for (auto& instance : mesh.second.instances)
        {
            VkDrawIndexedIndirectCommand command = {
                .indexCount = static_cast<u32>(mesh.second.indexCount),
                .instanceCount = 1,
                .firstIndex = static_cast<u32>(mesh.second.indexOffset),
                .vertexOffset = 0,
                .firstInstance = i++
            };
            cmds.push_back(command);
        }
    }
    backend.copyBufferWithStaging(
        cmds.data(), sizeof(VkDrawIndexedIndirectCommand) * cmds.size(), indirectCommands.buffer);
}

result::result<Scene, assetError> loadScene(VulkanBackend& backend, std::string name, std::string path,
    u32 lightCount)
{
    Scene scene = Scene(name, backend);

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    std::println("Loading {}", path);
    if (path.ends_with(".gltf"))
    {
        if (!loader.LoadASCIIFromFile(&model, &err, &warn, path))
        {
            std::println("{}", err);
            std::println("{}", warn);
            return result::fail(assetError{});
        }
    }
    else if (path.ends_with(".glb"))
    {
        if (!loader.LoadBinaryFromFile(&model, &err, &warn, path))
        {
            std::println("{}", err);
            std::println("{}", warn);
            return result::fail(assetError{});
        }
    }

    std::println("Successfully loaded {}", path);
    scene.addModel(model);
    scene.createBuffers();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<f32> uniformDistribution(0, 1);
    scene.pointLights.reserve(lightCount);

    scene.pointLights.push_back(PointLight {
        //.pos = glm::vec4(0, 30, 0, 1.f),
        //.color = glm::vec4(1.f, 0.6f, 0.2f, 1.f),
        //.range = glm::vec4(50.f),
        .pos = glm::vec4(22.7 * 0.005, 98.65 / 4.f * 0.005, 115.17 * 0.005, 1.f),
        //.pos = glm::vec4(520.f, 40.f, 0.f, 1.f),
        .color = glm::vec4(1.f, 0.95f, 0.8f, 1.f) * 10.f,
        //.range = glm::vec4(150.f),
        .range = glm::vec4(1.0f),
    });

    glm::vec3 exclusionAabb[2] = {
        glm::vec3(-900.f, 80.f, -200.f) * 0.005f,
        glm::vec3(750.f, 1300.f, 120.f) * 0.005f
    };

    auto diff = scene.aabbMax - scene.aabbMin;
    auto offsetMin = scene.aabbMin + diff * 0.15f;
    offsetMin.x -= diff.y * 0.05f;
    offsetMin.y -= diff.y * 0.1f;
    for (i32 i = 0; i < lightCount - 1; ++i)
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
            // std::println("Regening {} {} {}", pos.x, pos.y, pos.z);
            pos = glm::vec3(
                uniformDistribution(gen) * 0.75 * diff.x + offsetMin.x,
                uniformDistribution(gen) * 0.6 * diff.y + offsetMin.y,
                uniformDistribution(gen) * 0.65 * diff.z + offsetMin.z
            );
        } while (insideExclusion(pos));
        // std::println("\t OK {} {} {}", pos.x, pos.y, pos.z);

        scene.pointLights.push_back(PointLight{
            .pos = glm::vec4(pos, 1.f),
            .color = glm::vec4(
                uniformDistribution(gen) * 0.6 + 0.4,
                uniformDistribution(gen) * 0.6 + 0.4,
                uniformDistribution(gen) * 0.6 + 0.4,
                1.f
            ) * 10.f,
            .range = glm::vec4(uniformDistribution(gen) * 40 + 20) * 0.025f, // [20; 200]
        });
    }

    return scene;
}

Scene emptyScene(VulkanBackend& backend) { return Scene("empty", backend); }
