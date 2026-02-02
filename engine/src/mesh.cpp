#include "mesh.h"

#include "rhi/vulkan/backend.h"
#include "rhi/vulkan/utils/inits.h"

#include <print>
#include <numeric>

auto initModels(VulkanBackend& backend, u32 vertexSizeHint, u32 indexSizeHint, u32 instanceCountHint) -> Models
{
    const auto vertexInfo = vkutil::init::bufferCreateInfo(vertexSizeHint, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    const auto indexInfo = vkutil::init::bufferCreateInfo(indexSizeHint, VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto instanceInfo = vkutil::init::bufferCreateInfo(instanceCountHint * sizeof(Models::InstanceData),
                                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    std::println("Allocating vertex buffer {}B", vertexSizeHint);
    std::println("Allocating index buffer {}B", indexSizeHint);
    std::println("Allocating instance buffer {}B", instanceCountHint * sizeof(Models::InstanceData));

    return Models {
        .vertexBuffer = backend.allocateBuffer("Vertex buffer", vertexInfo, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .indexBuffer = backend.allocateBuffer("Index buffer", indexInfo, VMA_MEMORY_USAGE_GPU_ONLY,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .instanceBuffer = backend.allocateBuffer("Instance buffer", instanceInfo, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
}

auto loadModels(VulkanBackend& backend, Models& models, std::span<Models::ModelData> data, std::optional<std::span<Models::ModelDebug>> debugData) -> std::vector<ModelHandle>
{
    assert(!debugData.has_value() || debugData->size() == data.size());

    std::vector<Models::ModelDebug> generatedDebug;
    if (!debugData.has_value())
    {
        generatedDebug.reserve(data.size());
        
        static i32 genMeshNameCount = 0;
        for (size_t i{}; i < data.size(); ++i)
        {
            generatedDebug.push_back(Models::ModelDebug{std::format("genMesh{}", genMeshNameCount++)});
        }
        debugData = generatedDebug;
    }

    std::vector<ModelHandle> handles;
    handles.reserve(data.size());

    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    // for (const auto& modelData : data)
    for (ModelHandle i{}; i < data.size(); ++i)
    {
        const auto& modelData = data[i];
        
        std::optional<ModelHandle> handle = std::nullopt;
        for (ModelHandle i = 0; i < models.models.size(); i++)
        {
            if (models.models[i].hash == modelData.hash)
            {
                handle = i;
                break;
            }
        }

        if (handle)
        {
            handles.push_back(*handle);
            continue;
        }
        handle = models.models.size();
        handles.push_back(*handle);
                
        Models::Model model = {
            // .vertexOffset = vertices.size(),
            .vertexOffset = (models.vertexBytesWritten / sizeof(Vertex)) + vertices.size(),
            .vertexByteOffset = models.vertexBytesWritten,
            .vertexCount = modelData.vertices.size(),

            // .indexOffset = indices.size(),
            .indexOffset = (models.indexBytesWritten / sizeof(u32)) + indices.size(),
            .indexByteOffset = models.indexBytesWritten,
            .indexCount = modelData.indices.size(),

            .hash = modelData.hash,

            .firstInstance = 0,
            .instances = 0,
        };

        vertices.insert(vertices.end(), modelData.vertices.begin(), modelData.vertices.end());
        indices.insert(indices.end(), modelData.indices.begin(), modelData.indices.end());

        models.models.push_back(model);
        models.modelDebug.push_back((*debugData)[i]);
    }

    VkBufferCopy copyRegion {
        .srcOffset = 0,
        .dstOffset = models.vertexBytesWritten,
        .size = vertices.size() * sizeof(Vertex)
    };
    std::println("Writing {}B with dst offset {}", copyRegion.size, copyRegion.dstOffset);
    backend.copyBufferWithStaging(std::nullopt, vertices.data(), copyRegion.size, models.vertexBuffer.buffer, copyRegion);
    models.vertexBytesWritten += copyRegion.size;

    copyRegion = {
        .srcOffset = 0,
        .dstOffset = models.indexBytesWritten,
        .size = indices.size() * sizeof(u32)
    };
    backend.copyBufferWithStaging(std::nullopt, indices.data(), copyRegion.size, models.indexBuffer.buffer, copyRegion);
    models.indexBytesWritten += copyRegion.size;

    return handles;
}

auto addInstances(VulkanBackend& backend, Models& models, ModelHandle handle, std::span<Models::InstanceData> data, std::optional<std::span<Models::InstanceDebug>> debugData) -> std::vector<InstanceHandle>
{
    assert(!debugData.has_value() || debugData->size() == data.size());

    std::vector<Models::InstanceDebug> generatedDebug;
    if (!debugData.has_value())
    {
        generatedDebug.reserve(data.size());
        
        const auto& meshDebugName = models.modelDebug[handle].name;
        const auto instanceCount = models.models[handle].instances;
        for (size_t i{}; i < data.size(); ++i)
        {
            generatedDebug.push_back(Models::InstanceDebug{std::format("{}_instance{}", meshDebugName, i + instanceCount)});
        }
        debugData = generatedDebug;
    }

    std::vector<InstanceHandle> handles(data.size());
    std::iota(handles.begin(), handles.end(), models.instances[handle].size());

    models.instances[handle].insert(models.instances[handle].end(), data.begin(), data.end());
    // TODO: if sorting is done, this also needs to be sorted
    models.instanceDebug[handle].insert(models.instanceDebug[handle].end(), debugData->begin(), debugData->end());

    // TODO: no idea, but this might give a perf boost? 
    // Generally, probably a bad idea, because this makes the instance handles unstable
    // std::sort(models.instances[handle].begin(), models.instances[handle].end(),
    //     [](const Models::InstanceData& a, const Models::InstanceData& b)
    //     {
    //         return a.material.x < b.material.x;
    //     }
    // );
        
    std::vector<Models::InstanceData> flattenedInstances;
    for (ModelHandle i{}; i < models.models.size(); ++i)
    {
        Models::Model& model = models.models[i];
        model.firstInstance = flattenedInstances.size();
        model.instances = models.instances[i].size();
        flattenedInstances.insert(flattenedInstances.end(), models.instances[i].begin(), models.instances[i].end());
    }
    
    // TODO: we shouldn't be copying the entire buffer!
    const auto size = flattenedInstances.size() * sizeof(decltype(flattenedInstances)::value_type);
    std::println("Uploading instance data: {}B", size);
    backend.copyBufferWithStaging(std::nullopt, flattenedInstances.data(), size, models.instanceBuffer.buffer);

    return handles;
}

