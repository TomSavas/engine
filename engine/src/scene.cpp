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
    updateFreeCamera(dt, window, activeCamera);
}

void Scene::addMeshes(tinygltf::Model& model)
{
    // Matches Vertex definition
    const std::pair<std::string, int> attributes[] = {
        {"POSITION", 4},
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

            int vertexAttributeOffset = 0;
            for (const auto& [attribute, attributeCount] : attributes) {
                if (primitive.attributes.find(attribute) == primitive.attributes.end()) 
                {
                    continue;
                }

                tinygltf::Accessor accessor = model.accessors[primitive.attributes[attribute]];
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
            }
        }
    }
}
