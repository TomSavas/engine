#include "debugShapes.h"
#include <initializer_list>

#include "rhi/vulkan/backend.h"
#include "scene.h"

template<>
struct std::hash<Vertex>
{
    std::size_t operator()(const Vertex& v) const noexcept
    {
        auto hash = static_cast<u64>(v.raw[0]);
        for (u32 i = 1; i < 16; i++)
        {
            hash ^= static_cast<u64>(v.raw[i]) + static_cast<u64>(0x9e3779b9) + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

auto loadMesh(const std::string& path) -> Models::ModelData
{
    Models::ModelData data;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    if (!loader.LoadASCIIFromFile(&model, &err, &warn, path))
    {
        // std::println("{}", err);
        // std::println("{}", warn);
        return data;
    }

    tinygltf::Mesh mesh = model.meshes[0];
    
    // Matches Vertex definition
    const char* position = "POSITION";
    const std::pair<const char*, i32> attributes[] = {
        {position, 4},
        {"TEXCOORD_0", 4},
        {"NORMAL", 4},
        {"TANGENT", 4},
    };

    i32 primitiveCount = 0;
    for (tinygltf::Primitive& primitive : mesh.primitives)
    {
        i32 vertexAttributeOffset = 0;
        for (const auto& [attribute, attributeCount] : attributes)
        {
            if (primitive.attributes.find(attribute) == primitive.attributes.end())
            {
                continue;
            }

            tinygltf::Accessor accessor = model.accessors[primitive.attributes[std::string(attribute)]];
            tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            f32* rawData = reinterpret_cast<f32*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);

            for (i32 i = 0; i < accessor.count; i++)
            {
                i32 vertexIndex = i;
                if (data.vertices.size() <= vertexIndex)
                {
                    data.vertices.emplace_back();
                }
                Vertex& vertex = data.vertices[vertexIndex];

                const i32 componentCount = tinygltf::GetNumComponentsInType(accessor.type);
                for (i32 j = 0; j < componentCount; j++)
                {
                    vertex.raw[vertexAttributeOffset + j] = rawData[i * componentCount + j];
                }
            }
            vertexAttributeOffset += attributeCount;
        }

        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];
        tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
        const unsigned short* indexData = reinterpret_cast<unsigned short*>(
            &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
        for (size_t i = 0; i < indexAccessor.count; i++)
        {
            data.indices.push_back(indexData[i]);
        }
    }

    return data;
}

auto cubeModelData() -> Models::ModelData
{
    // Models::ModelData data = loadMesh("../assets/Sponza/Sponza.gltf");
    // Models::ModelData data = loadMesh("../assets/Box/Box.gltf");
    // Models::ModelData data = loadMesh("../assets/Suzanne/Suzanne.gltf");
    // Models::ModelData data = loadMesh("../assets/box/Box.gltf");
    Models::ModelData data = loadMesh("../assets/Cube/Cube.gltf");
    // data.vertices.insert({
    //     Vertex
    //     {
    //         .pos = {},
    //         .uv = {},
    //         .normal = {},
    //         .tangent = {},
    //     },
    //     Vertex
    //     {
    //         .pos = {},
    //         .uv = {},
    //         .normal = {},
    //         .tangent = {},
    //     },
    //     Vertex
    //     {
    //         .pos = {},
    //         .uv = {},
    //         .normal = {},
    //         .tangent = {},
    //     },
    //     Vertex
    //     {
    //         .pos = {},
    //         .uv = {},
    //         .normal = {},
    //         .tangent = {},
    //     },
    //     Vertex
    //     {
    //         .pos = {},
    //         .uv = {},
    //         .normal = {},
    //         .tangent = {},
    //     },
    //     Vertex
    //     {
    //         .pos = {},
    //         .uv = {},
    //         .normal = {},
    //         .tangent = {},
    //     },
    //     Vertex
    //     {
    //         .pos = {},
    //         .uv = {},
    //         .normal = {},
    //         .tangent = {},
    //     },
    //     Vertex
    //     {
    //         .pos = {},
    //         .uv = {},
    //         .normal = {},
    //         .tangent = {},
    //     },
    // });
    // data.indices.insert({
    //     1, 0, 3, 3, 2, 1// top                        
    //     // bottom
    //     2, 3, 7, 7, 6, 2// near
    //     // far
    //     // left
    //     1, 2, 6, 6, 5, 1// right
    // });

    // TEMP: this should be hard-coded

    for (const auto& vertex : data.vertices)
    {
        data.hash ^= std::hash<Vertex>{}(vertex) + static_cast<u64>(0x9e3779b9) + (data.hash << 6) + (data.hash >> 2);
    }
    for (const auto& index : data.indices)
    {
        data.hash ^= static_cast<u64>(index) + static_cast<u64>(0x9e3779b9) + (data.hash << 6) + (data.hash >> 2);
    }

    return data;
}

auto sphereModelData(u8 horizontalSubdivisions, u8 verticalSubdivisions) -> Models::ModelData
{
    Models::ModelData data;

    horizontalSubdivisions = std::max<u8>(horizontalSubdivisions, 2);
    verticalSubdivisions = std::max<u8>(verticalSubdivisions, 3);

    const f32 radius = 0.5f;
    const f32 pi = std::numbers::pi_v<f32>;

    data.vertices.reserve((horizontalSubdivisions + 1) * (verticalSubdivisions + 1));
    data.indices.reserve(horizontalSubdivisions * verticalSubdivisions * 6);

    for (u8 i = 0; i <= horizontalSubdivisions; ++i)
    {
        f32 v = 1.0f - static_cast<f32>(i) / horizontalSubdivisions;
        f32 phi = static_cast<f32>(i) / horizontalSubdivisions * pi;

        for (u8 j = 0; j <= verticalSubdivisions; ++j)
        {
            f32 u = static_cast<f32>(j) / verticalSubdivisions;
            f32 theta = u * 2.0f * pi;

            f32 x = -radius * std::sin(phi) * std::sin(theta);
            f32 y = radius * std::cos(phi);
            f32 z = radius * std::sin(phi) * std::cos(theta);

            f32 nx = x / radius;
            f32 ny = y / radius;
            f32 nz = z / radius;

            f32 tx = -std::cos(theta);
            f32 ty = 0.0f;
            f32 tz = -std::sin(theta);

            data.vertices.push_back(Vertex{
                .pos = {x, y, z, 1.0f},
                .uv = {u, v},
                .normal = {nx, ny, nz, 0.0f},
                .tangent = {tx, ty, tz, 0.0f}
            });
        }
    }

    for (u8 i = 0; i < horizontalSubdivisions; ++i)
    {
        for (u8 j = 0; j < verticalSubdivisions; ++j)
        {
            u8 topLeft = i * (verticalSubdivisions + 1) + j;
            u8 topRight = topLeft + 1;
            u8 bottomLeft = (i + 1) * (verticalSubdivisions + 1) + j;
            u8 bottomRight = bottomLeft + 1;

            data.indices.push_back(topLeft);
            data.indices.push_back(topRight);
            data.indices.push_back(bottomLeft);

            data.indices.push_back(topRight);
            data.indices.push_back(bottomRight);
            data.indices.push_back(bottomLeft);
        }
    }
    
    for (const auto& vertex : data.vertices)
    {
        data.hash ^= std::hash<Vertex>{}(vertex) + static_cast<u64>(0x9e3779b9) + (data.hash << 6) + (data.hash >> 2);
    }
    for (const auto& index : data.indices)
    {
        data.hash ^= static_cast<u64>(index) + static_cast<u64>(0x9e3779b9) + (data.hash << 6) + (data.hash >> 2);
    }

    return data;
}

auto planeModelData() -> Models::ModelData
{
    Models::ModelData data;
    data.vertices.insert(data.vertices.begin(),
        {
            Vertex
            {
                .pos = {-0.5f, 0.f, 0.5f, 1.f},
                .uv = {0.f, 1.f},
                .normal = {0.f, 1.f, 0.f, 0.f},
                .tangent = {1.f, 0.f, 0.f, 0.f},
            },
            Vertex
            {
                .pos = {0.5f, 0.f, 0.5f, 1.f},
                .uv = {1.f, 1.f},
                .normal = {0.f, 1.f, 0.f, 0.f},
                .tangent = {1.f, 0.f, 0.f, 0.f},
            },
            Vertex
            {
                .pos = {0.5f, 0.f, -0.5f, 1.f},
                .uv = {1.f, 0.f},
                .normal = {0.f, 1.f, 0.f, 0.f},
                .tangent = {1.f, 0.f, 0.f, 0.f},
            },
            Vertex
            {
                .pos = {-0.5f, 0.f, -0.5f, 1.f},
                .uv = {0.f, 0.f},
                .normal = {0.f, 1.f, 0.f, 0.f},
                .tangent = {1.f, 0.f, 0.f, 0.f},
            },
        });
    data.indices.insert(data.indices.begin(),
        {
            0, 1, 3,
            1, 2, 3,
        });

    for (const auto& vertex : data.vertices)
    {
        data.hash ^= std::hash<Vertex>{}(vertex) + static_cast<u64>(0x9e3779b9) + (data.hash << 6) + (data.hash >> 2);
    }
    for (const auto& index : data.indices)
    {
        data.hash ^= static_cast<u64>(index) + static_cast<u64>(0x9e3779b9) + (data.hash << 6) + (data.hash >> 2);
    }

    return data;
}

auto blankMat(std::array<f32, 4> color) -> DefaultMaterial
{
    return DefaultMaterial {
        .albedo = BindlessResources::kWhite,
        .normalTexture = BindlessResources::kWhite,
        .metallicRoughnessTexture = BindlessResources::kWhite,
        .bumpTexture = BindlessResources::kWhite,
        .baseColor = {color[0], color[1], color[2], color[3]},
        // .features = DefaultMaterial::Features::ALL,
        .uvScaleOffset = {1.f, 1.f, 0.f, 0.f},
        .features = DefaultMaterial::Features::WIREFRAME,
    };
}

auto debugDrawCube(Scene& scene, glm::vec3 center, glm::vec3 scale, glm::vec3 color) -> void
{
    std::array mat {
        blankMat({color.r, color.g, color.b, 1.f})
    };
    const auto materials = addMaterials<DefaultMaterial>(scene.backend, scene.materials, mat);

    std::array models = {
        cubeModelData()
    };
    std::array modelDebugs = {
        Models::ModelDebug{"cube"}
    };
    const auto modelHandles = loadModels(scene.backend, scene.models, models, modelDebugs);

    glm::mat4 transform = glm::scale(glm::translate(glm::mat4(1.f), center), scale);
    std::array instances {
        Models::InstanceData {
                .transform = transform,
                .material = glm::vec4(materials.back()),
        }
    };
    const auto instanceHandles = addInstances(scene.backend, scene.models, modelHandles.back(), instances);

    std::vector<SceneGraph::NodeHandle> rootHandle {SceneGraph::kRootHandle};
    for (auto nodeHandle : scene.sceneGraph.addEmptyChildNodes(rootHandle))
    {
        auto& node = scene.sceneGraph.nodes[nodeHandle];
        node.localTransform = glm::mat4(1.f);
        node.globalTransform = transform;
        node.dirty = SceneGraph::NewNode::TransformDirtiness::LOCAL_DIRTY;
        node.model = modelHandles.back();
        node.instance = instanceHandles.back();
    }
}

auto debugDrawSphere(Scene& scene, glm::vec3 center, glm::vec3 scale, glm::vec3 color) -> void
{
    std::array mat {
        DefaultMaterial {
            .albedo = BindlessResources::kWhite,
            .normalTexture = BindlessResources::kWhite,
            .metallicRoughnessTexture = BindlessResources::kWhite,
            .bumpTexture = BindlessResources::kWhite,
            .baseColor = {color.r, color.g, color.b, 1.f},
            .uvScaleOffset = {1.f, 1.f, 0.f, 0.f},
            .features = DefaultMaterial::Features::LIT,
        }
    };
    const auto materials = addMaterials<DefaultMaterial>(scene.backend, scene.materials, mat);

    std::array models = {
        sphereModelData()
    };
    std::array modelDebugs = {
        Models::ModelDebug{"sphere"}
    };
    const auto modelHandles = loadModels(scene.backend, scene.models, models, modelDebugs);

    glm::mat4 transform = glm::scale(glm::translate(glm::mat4(1.f), center), scale);
    std::array instances {
        Models::InstanceData {
                .transform = transform,
                .material = glm::vec4(materials.back()),
        }
    };
    const auto instanceHandles = addInstances(scene.backend, scene.models, modelHandles.back(), instances);

    std::vector<SceneGraph::NodeHandle> rootHandle {SceneGraph::kRootHandle};
    for (auto nodeHandle : scene.sceneGraph.addEmptyChildNodes(rootHandle))
    {
        auto& node = scene.sceneGraph.nodes[nodeHandle];
        node.localTransform = glm::mat4(1.f);
        node.globalTransform = transform;
        node.dirty = SceneGraph::NewNode::TransformDirtiness::LOCAL_DIRTY;
        node.model = modelHandles.back();
        node.instance = instanceHandles.back();
    }
    
}

auto debugDrawPlane(Scene& scene, glm::vec3 center, glm::vec3 scale, glm::vec3 color) -> SceneGraph::NodeHandle
{
    std::array mat {
        DefaultMaterial {
            .albedo = BindlessResources::kError,
            .normalTexture = BindlessResources::kWhite,
            .metallicRoughnessTexture = BindlessResources::kWhite,
            .bumpTexture = BindlessResources::kWhite,
            .baseColor = {1.f, 1.f, 1.f, 1.f},
            .uvScaleOffset = {1.f, 1.f, 0.f, 0.f},
            .features = DefaultMaterial::Features::LIT,
        }
    };
    const auto materials = addMaterials<DefaultMaterial>(scene.backend, scene.materials, mat);

    std::array models = {
        planeModelData()
    };
    std::array modelDebugs = {
        Models::ModelDebug{"plane"}
    };
    const auto modelHandles = loadModels(scene.backend, scene.models, models, modelDebugs);

    glm::mat4 transform = glm::scale(glm::translate(glm::mat4(1.f), center), scale);
    std::array instances {
        Models::InstanceData {
                .transform = transform,
                .material = glm::vec4(materials.back()),
        }
    };
    const auto instanceHandles = addInstances(scene.backend, scene.models, modelHandles.back(), instances);

    std::vector<SceneGraph::NodeHandle> rootHandle {SceneGraph::kRootHandle};

    auto nodeHandle = scene.sceneGraph.addEmptyChildNodes(rootHandle).back();
    auto& node = scene.sceneGraph.nodes[nodeHandle];
    node.localTransform = glm::mat4(1.f);
    node.globalTransform = transform;
    node.dirty = SceneGraph::NewNode::TransformDirtiness::LOCAL_DIRTY;
    node.model = modelHandles.back();
    node.instance = instanceHandles.back();

    return nodeHandle;
}
