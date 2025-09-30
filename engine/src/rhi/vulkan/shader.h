#pragma once

#include "engine.h"

#include "vulkan/vulkan.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#define SHADER_SRC_PATH "../src/shaders/"
#define SHADER_SPIRV_PATH "./shaders/"
#define SHADER_SPIRV_EXTENSION ".spv"

#define SHADER_FILENAME_AND_SPIRV_AND_SRC_PATH(filename) \
    filename, SHADER_SPIRV_PATH filename SHADER_SPIRV_EXTENSION, SHADER_SRC_PATH filename

struct ShaderPath
{
    std::string filename;
    std::string spirvPath;
    std::string sourcePath;

    // File timestamps? -> mv ShaderFileInfo

    explicit ShaderPath(std::string filename, std::string spirvPath, std::string sourcePath)
        : filename(filename), spirvPath(spirvPath), sourcePath(sourcePath)
    {
    }

    bool operator==(const ShaderPath& other) const { return spirvPath.compare(other.spirvPath) == 0; }

    auto hash() const -> size_t { return std::hash<std::string>{}(spirvPath); }

    struct Hash
    {
        auto operator()(const ShaderPath& shaderPath) const -> size_t { return shaderPath.hash(); }
    };
};
#define SHADER_PATH(filename) ShaderPath(SHADER_FILENAME_AND_SPIRV_AND_SRC_PATH(filename))

struct ShaderModule
{
    ShaderPath path;

    std::vector<u32> spirvCode;

    VkShaderModule module;

    explicit ShaderModule(ShaderPath path) : path(path) {}
};

struct ShaderModuleCache
{
    // TODO: watcher to reload shaders
    std::unordered_map<ShaderPath, ShaderModule, ShaderPath::Hash> cache;

    auto loadModule(VkDevice device, ShaderPath path) -> std::optional<ShaderModule*>;
};
