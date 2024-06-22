#pragma once

#include <vulkan/vulkan.h>

#include <vector>

struct PipelineBuilder
{
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo multisampling;
    //VkPipelineLayout pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo depthStencil;
    VkPipelineRenderingCreateInfo renderInfo;
    std::vector<VkFormat> colorAttachments;

    PipelineBuilder();

    void reset();

    PipelineBuilder& shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    PipelineBuilder& topology(VkPrimitiveTopology topology);
    PipelineBuilder& polyMode(VkPolygonMode polyMode);
    PipelineBuilder& cullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    PipelineBuilder& disableMultisampling();
    PipelineBuilder& disableBlending();
    PipelineBuilder& enableAlphaBlending();
    PipelineBuilder& colorAttachmentFormat(VkFormat format);
    PipelineBuilder& depthFormat(VkFormat format);
    PipelineBuilder& disableDepthTest();
        
    VkPipeline build(VkDevice device, VkPipelineLayout layout);
};
