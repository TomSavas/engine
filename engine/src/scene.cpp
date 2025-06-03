#include "scene.h"

#include "tracy/Tracy.hpp"

#include "GLFW/glfw3.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <atomic>
#include <algorithm>
#include <print>

glm::vec3 right(glm::mat4 mat) {
    return mat * glm::vec4(1.f, 0.f, 0.f, 0.f);
}

glm::vec3 up(glm::mat4 mat) {
    return mat * glm::vec4(0.f, 1.f, 0.f, 0.f);
}

glm::vec3 forward(glm::mat4 mat) {
    return mat * glm::vec4(0.f, 0.f, -1.f, 0.f);
}

static std::atomic<double> yOffsetAtomic;
void scrollCallback(GLFWwindow* window, double xoffset, double yOffset)
{
    yOffsetAtomic.store(yOffset);
}

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

    static glm::dvec2 lastMousePos = glm::vec2(-1.f -1.f);
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
        while(radToVertical > glm::pi<float>() * 2) 
        {
            radToVertical -= glm::pi<float>() * 2;
        }
        while(radToVertical < -glm::pi<float>() * 2) 
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
            for (const auto& [attribute, attributeCount] : attributes) {
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
                // std::println("{} {} {}", vertexData.back().pos[0], vertexData.back().pos[1], vertexData.back().pos[2]);
                vertexAttributeOffset += attributeCount;
            }

            tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];
            tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
            const unsigned short* indexData = reinterpret_cast<unsigned short*>(&indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
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
                Texture t = backend.textures.loadRaw(albedoImg.image.data(), albedoImg.image.size(),
                    albedoImg.width, albedoImg.height, true, false, albedoImg.name);
                bindlessImages.push_back(backend.bindless.addTexture(t));
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
                
                // TODO: remove above
                Texture t = backend.textures.loadRaw(normalImg.image.data(), normalImg.image.size(),
                    normalImg.width, normalImg.height, true, false, normalImg.name);
                bindlessImages.push_back(backend.bindless.addTexture(t));
                m.normalTexture = bindlessImages.back();
            }
        }
    }
}
