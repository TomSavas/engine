#include "rhi/vulkan/shader.h"

#include <fstream>
#include <print>

std::vector<uint32_t> readFile(std::string path)
{
    std::vector<uint32_t> buffer;

    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (file.is_open())
    {
        size_t fileSize = (size_t)file.tellg();
        buffer.resize(fileSize / sizeof(uint32_t));

        file.seekg(0);
        file.read((char*)buffer.data(), fileSize);
        file.close();
    }

    return buffer;
}

std::optional<ShaderModule*> ShaderModuleCache::loadModule(VkDevice device, ShaderPath path)
{
    auto moduleFromCache = cache.find(path);
    if (moduleFromCache != cache.end())
    {
        std::println("Loading cached module {} - {:x}", path.filename, path.hash());
        return &moduleFromCache->second;
    }
    std::println("Loading new module {} from {} (src: {}) - {:x}\n", path.filename, path.spirvPath, path.sourcePath,
        path.hash());

    cache.emplace(path, ShaderModule(path));
    ShaderModule& module = cache.at(path);
    module.spirvCode = readFile(path.spirvPath);
    // TODO: add source code here for debugging

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    createInfo.codeSize = module.spirvCode.size() * sizeof(uint32_t);
    createInfo.pCode = module.spirvCode.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        std::println("Failed creating a shader module for {}", path.filename);
        // TODO: remove from cache
        return {};
    }
    module.module = shaderModule;

    return &module;
}
