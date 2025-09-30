#pragma once

#include "rhi/vulkan/shader.h"

#include <vulkan/vulkan.h>

#include <optional>
#include <vector>

class VulkanBackend;

struct Pipeline
{
    VkPipelineBindPoint pipelineBindPoint;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
};

enum class PipelineError
{
    ShaderError,
    PipelineLayoutCreateError,
    PipelineCreateError,
};

struct PipelineBuilder
{
    VulkanBackend& backend;
    std::optional<PipelineError> builderError;

    VkPipelineBindPoint determinedBindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;

    struct LayoutInfo
    {
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
        std::vector<VkPushConstantRange> pushConstants;
    } layoutInfo;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineDepthStencilStateCreateInfo depthStencil;
    VkPipelineRenderingCreateInfo renderInfo;
    std::vector<VkFormat> colorAttachments;
    std::vector<VkDynamicState> dynamicStates;

    explicit PipelineBuilder(VulkanBackend& backend);

    auto reset() -> void;

    auto addDescriptorLayouts(std::initializer_list<VkDescriptorSetLayout>&& descriptorSetLayouts) -> PipelineBuilder&;
    auto addPushConstants(std::initializer_list<VkPushConstantRange>&& pushConstants) -> PipelineBuilder&;
    auto addShader(ShaderPath path, VkShaderStageFlagBits stage) -> PipelineBuilder&;
    auto topology(VkPrimitiveTopology topology) -> PipelineBuilder&;
    auto polyMode(VkPolygonMode polyMode) -> PipelineBuilder&;
    auto cullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) -> PipelineBuilder&;
    auto disableMultisampling() -> PipelineBuilder&;
    auto disableBlending() -> PipelineBuilder&;
    auto enableAlphaBlending() -> PipelineBuilder&;
    auto colorAttachmentFormat(VkFormat format) -> PipelineBuilder&;
    auto depthFormat(VkFormat format) -> PipelineBuilder&;
    auto enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp) -> PipelineBuilder&;
    auto disableDepthTest() -> PipelineBuilder&;
    auto setDepthClamp(bool enable) -> PipelineBuilder&;
    auto addDynamicState(VkDynamicState state) -> PipelineBuilder&;
    auto addViewportScissorDynamicStates() -> PipelineBuilder&;
    auto enableDepthBias() -> PipelineBuilder&;

    auto buildLayout() -> VkPipelineLayout;
    auto buildGraphicsPipeline(VkPipelineLayout layout) -> VkPipeline;
    auto buildComputePipeline(VkPipelineLayout layout) -> VkPipeline;
    auto build() -> Pipeline;
};
