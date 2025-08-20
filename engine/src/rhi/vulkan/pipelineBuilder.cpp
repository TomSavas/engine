#include "rhi/vulkan/pipelineBuilder.h"

#include <vulkan/vulkan_core.h>

#include <print>

#include "backend.h"
#include "rhi/vulkan/utils/inits.h"
#include "shader.h"

PipelineBuilder::PipelineBuilder(VulkanBackend& backend) : backend(backend) { reset(); }

void PipelineBuilder::reset()
{
    inputAssembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    colorBlendAttachment = {};
    multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    // pipelineLayout = {};
    depthStencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 0,
        .pColorAttachmentFormats = nullptr};

    shaderStages.clear();
    colorAttachments.clear();
}


PipelineBuilder& PipelineBuilder::addDescriptorLayouts(
    std::initializer_list<VkDescriptorSetLayout>&& descriptorSetLayouts)
{
    layoutInfo.descriptorSetLayouts.insert(layoutInfo.descriptorSetLayouts.end(),
        std::make_move_iterator(descriptorSetLayouts.begin()),
        std::make_move_iterator(descriptorSetLayouts.end()));

    return *this;
}

PipelineBuilder& PipelineBuilder::addPushConstants(std::initializer_list<VkPushConstantRange>&& pushConstants)
{
    layoutInfo.pushConstants.insert(layoutInfo.pushConstants.end(),
        std::make_move_iterator(pushConstants.begin()),
        std::make_move_iterator(pushConstants.end()));

    return *this;
}

PipelineBuilder& PipelineBuilder::addShader(ShaderPath path, VkShaderStageFlagBits stage)
{
    std::optional<ShaderModule*> vertexShader = backend.shaderModuleCache.loadModule(backend.device, path);
    if (!vertexShader)
    {
        builderError = PipelineError::ShaderError;
        return *this;
    }

    VkPipelineBindPoint newDeterminedBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if (stage == VK_SHADER_STAGE_COMPUTE_BIT)
    {
        newDeterminedBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    }

    const bool bindPointSet = determinedBindPoint != VK_PIPELINE_BIND_POINT_MAX_ENUM;
    if (bindPointSet && newDeterminedBindPoint != determinedBindPoint)
    {
        std::println("Setting shader {} with stage {} results in changing pipeline bind point from {} to {}."
                     " Operation ignored.", path.sourcePath, (int)stage, (int)determinedBindPoint, (int)newDeterminedBindPoint);
        return *this;
    }
    determinedBindPoint = newDeterminedBindPoint;

    shaderStages.push_back(vkutil::init::shaderStageCreateInfo(stage, (*vertexShader)->module));

    return *this;
}

VkPipelineLayout PipelineBuilder::buildLayout()
{
    VkPipelineLayout layout;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkutil::init::layoutCreateInfo(
layoutInfo.descriptorSetLayouts.data(), layoutInfo.descriptorSetLayouts.size(), layoutInfo.pushConstants.data(),
layoutInfo.pushConstants.size());
    if (const VkResult error = vkCreatePipelineLayout(backend.device, &pipelineLayoutInfo, nullptr, &layout))
    {
        builderError = PipelineError::PipelineLayoutCreateError;
        VK_CHECK(error);
    }

    return layout;
}

VkPipeline PipelineBuilder::buildGraphicsPipeline(VkPipelineLayout layout)
{
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .scissorCount = 1
    };

    // setup dummy color blending. We aren't using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicInfo,
        .layout = layout
    };

    VkPipeline pipeline;
    if (const VkResult error = vkCreateGraphicsPipelines(backend.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
        &pipeline))
    {
        builderError = PipelineError::PipelineCreateError;
        VK_CHECK(error);
    }

    return pipeline;
}

VkPipeline PipelineBuilder::buildComputePipeline(VkPipelineLayout layout)
{
    VkPipeline pipeline;
    VkComputePipelineCreateInfo pipelineCreateInfo = vkutil::init::computePipelineCreateInfo(layout, shaderStages[0]);
    if (const VkResult error = vkCreateComputePipelines(backend.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
        nullptr, &pipeline))
    {
        builderError = PipelineError::PipelineCreateError;
        VK_CHECK(error);
    }

    return pipeline;
}

Pipeline PipelineBuilder::build()
{
    VkPipelineLayout layout = buildLayout();
    return Pipeline {
        .pipelineBindPoint = determinedBindPoint,
        .pipelineLayout = layout,
        .pipeline = determinedBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
            ? buildGraphicsPipeline(layout)
            : buildComputePipeline(layout)
    };
}




















PipelineBuilder& PipelineBuilder::shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    shaderStages.push_back(vkutil::init::shaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
    shaderStages.push_back(vkutil::init::shaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));

    return *this;
}

PipelineBuilder& PipelineBuilder::shaders(
    VkShaderModule vertexShader, VkShaderModule geometryShader, VkShaderModule fragmentShader)
{
    shaderStages.push_back(vkutil::init::shaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
    shaderStages.push_back(vkutil::init::shaderStageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT, geometryShader));
    shaderStages.push_back(vkutil::init::shaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));

    return *this;
}

PipelineBuilder& PipelineBuilder::topology(VkPrimitiveTopology topology)
{
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = false;

    return *this;
}

PipelineBuilder& PipelineBuilder::polyMode(VkPolygonMode polyMode)
{
    rasterizer.polygonMode = polyMode;
    rasterizer.lineWidth = 1.f;

    return *this;
}

PipelineBuilder& PipelineBuilder::cullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    rasterizer.cullMode = cullMode;
    rasterizer.frontFace = frontFace;

    return *this;
}

PipelineBuilder& PipelineBuilder::disableMultisampling()
{
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    return *this;
}

PipelineBuilder& PipelineBuilder::disableBlending()
{
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    return *this;
}

PipelineBuilder& PipelineBuilder::enableAlphaBlending()
{
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    return *this;
}

PipelineBuilder& PipelineBuilder::colorAttachmentFormat(VkFormat format)
{
    colorAttachments.push_back(format);

    renderInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    renderInfo.pColorAttachmentFormats = colorAttachments.data();

    return *this;
}

PipelineBuilder& PipelineBuilder::depthFormat(VkFormat format)
{
    renderInfo.depthAttachmentFormat = format;

    return *this;
}

PipelineBuilder& PipelineBuilder::enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp)
{
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = depthWriteEnable;
    depthStencil.depthCompareOp = compareOp;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};
    depthStencil.minDepthBounds = 0.f;
    depthStencil.maxDepthBounds = 1.f;

    return *this;
}

PipelineBuilder& PipelineBuilder::disableDepthTest()
{
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};
    depthStencil.minDepthBounds = 0.f;
    depthStencil.maxDepthBounds = 1.f;

    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthClamp(bool enable)
{
    rasterizer.depthClampEnable = enable;

    return *this;
}

PipelineBuilder& PipelineBuilder::addDynamicState(VkDynamicState state)
{
    dynamicStates.push_back(state);
    return *this;
}

PipelineBuilder& PipelineBuilder::addViewportScissorDynamicStates()
{
    dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
    return *this;
}

PipelineBuilder& PipelineBuilder::enableDepthBias()
{
    rasterizer.depthBiasEnable = VK_TRUE;
    dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    return *this;
}

VkPipeline PipelineBuilder::build(VkDevice device, VkPipelineLayout layout)
{
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .scissorCount = 1
    };

    // setup dummy color blending. We aren't using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicInfo,
        .layout = layout
    };

    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS)
    {
        std::println("Failed creating pipeline");
        newPipeline = VK_NULL_HANDLE;
    }

    return newPipeline;
}
