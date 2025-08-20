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

    VkPipelineLayout buildLayout();
    VkPipeline buildGraphicsPipeline(VkPipelineLayout layout);
    VkPipeline buildComputePipeline(VkPipelineLayout layout);
    Pipeline build();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineDepthStencilStateCreateInfo depthStencil;
    VkPipelineRenderingCreateInfo renderInfo;
    std::vector<VkFormat> colorAttachments;
    std::vector<VkDynamicState> dynamicStates;

    PipelineBuilder(VulkanBackend& backend);

    void reset();

    PipelineBuilder& addDescriptorLayouts(std::initializer_list<VkDescriptorSetLayout>&& descriptorSetLayouts);
    PipelineBuilder& addPushConstants(std::initializer_list<VkPushConstantRange>&& pushConstants);
    PipelineBuilder& addShader(ShaderPath path, VkShaderStageFlagBits stage);

    PipelineBuilder& shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    PipelineBuilder& shaders(VkShaderModule vertexShader, VkShaderModule geometryShader, VkShaderModule fragmentShader);
    PipelineBuilder& topology(VkPrimitiveTopology topology);
    PipelineBuilder& polyMode(VkPolygonMode polyMode);
    PipelineBuilder& cullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    PipelineBuilder& disableMultisampling();
    PipelineBuilder& disableBlending();
    PipelineBuilder& enableAlphaBlending();
    PipelineBuilder& colorAttachmentFormat(VkFormat format);
    PipelineBuilder& depthFormat(VkFormat format);
    PipelineBuilder& enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp);
    PipelineBuilder& disableDepthTest();
    PipelineBuilder& setDepthClamp(bool enable);
    PipelineBuilder& addDynamicState(VkDynamicState state);
    PipelineBuilder& addViewportScissorDynamicStates();
    PipelineBuilder& enableDepthBias();

    VkPipeline build(VkDevice device, VkPipelineLayout layout);
};
