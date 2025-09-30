#include "rhi/vulkan/pipelineBuilder.h"

#include "backend.h"
#include "rhi/vulkan/utils/inits.h"
#include "rhi/vulkan/vulkan.h"
#include "shader.h"

#include <vulkan/vulkan_core.h>

#include <print>

PipelineBuilder::PipelineBuilder(VulkanBackend& backend) : backend(backend) { reset(); }

auto PipelineBuilder::reset() -> void
{
    inputAssembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    // pipelineLayout = {};
    depthStencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 0,
        .pColorAttachmentFormats = nullptr};

    shaderStages.clear();
    colorAttachments.clear();
    colorBlendAttachments.clear();
}


auto PipelineBuilder::addDescriptorLayouts(std::initializer_list<VkDescriptorSetLayout>&& descriptorSetLayouts)
    -> PipelineBuilder&
{
    layoutInfo.descriptorSetLayouts.insert(layoutInfo.descriptorSetLayouts.end(),
        std::make_move_iterator(descriptorSetLayouts.begin()),
        std::make_move_iterator(descriptorSetLayouts.end()));

    return *this;
}

auto PipelineBuilder::addPushConstants(std::initializer_list<VkPushConstantRange>&& pushConstants) -> PipelineBuilder&
{
    layoutInfo.pushConstants.insert(layoutInfo.pushConstants.end(),
        std::make_move_iterator(pushConstants.begin()),
        std::make_move_iterator(pushConstants.end()));

    return *this;
}

auto PipelineBuilder::addShader(ShaderPath path, VkShaderStageFlagBits stage) -> PipelineBuilder&
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
                     " Operation ignored.", path.sourcePath, (i32)stage, (i32)determinedBindPoint, (i32)newDeterminedBindPoint);
        return *this;
    }
    determinedBindPoint = newDeterminedBindPoint;

    shaderStages.push_back(vkutil::init::shaderStageCreateInfo(stage, (*vertexShader)->module));

    return *this;
}

auto PipelineBuilder::buildLayout() -> VkPipelineLayout
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

auto PipelineBuilder::buildGraphicsPipeline(VkPipelineLayout layout) -> VkPipeline
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
        .attachmentCount = colorBlendAttachments.size(),
        .pAttachments = colorBlendAttachments.data()
    };

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .dynamicStateCount = static_cast<u32>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .stageCount = static_cast<u32>(shaderStages.size()),
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

auto PipelineBuilder::buildComputePipeline(VkPipelineLayout layout) -> VkPipeline
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

auto PipelineBuilder::build() -> Pipeline
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

auto PipelineBuilder::topology(VkPrimitiveTopology topology) -> PipelineBuilder&
{
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = false;

    return *this;
}

auto PipelineBuilder::polyMode(VkPolygonMode polyMode) -> PipelineBuilder&
{
    rasterizer.polygonMode = polyMode;
    rasterizer.lineWidth = 1.f;

    return *this;
}

auto PipelineBuilder::cullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) -> PipelineBuilder&
{
    rasterizer.cullMode = cullMode;
    rasterizer.frontFace = frontFace;

    return *this;
}

auto PipelineBuilder::disableMultisampling() -> PipelineBuilder&
{
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    return *this;
}

auto PipelineBuilder::disableBlending() -> PipelineBuilder&
{
    colorBlendAttachments.push_back({
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    });

    return *this;
}

auto PipelineBuilder::enableAlphaBlending() -> PipelineBuilder&
{
    colorBlendAttachments.push_back({
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    });

    return *this;
}

auto PipelineBuilder::colorAttachmentFormat(VkFormat format) -> PipelineBuilder&
{
    colorAttachments.push_back(format);

    renderInfo.colorAttachmentCount = static_cast<u32>(colorAttachments.size());
    renderInfo.pColorAttachmentFormats = colorAttachments.data();

    return *this;
}

auto PipelineBuilder::depthFormat(VkFormat format) -> PipelineBuilder&
{
    renderInfo.depthAttachmentFormat = format;

    return *this;
}

auto PipelineBuilder::enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp) -> PipelineBuilder&
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

auto PipelineBuilder::disableDepthTest() -> PipelineBuilder&
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

auto PipelineBuilder::setDepthClamp(bool enable) -> PipelineBuilder&
{
    rasterizer.depthClampEnable = enable;

    return *this;
}

auto PipelineBuilder::addDynamicState(VkDynamicState state) -> PipelineBuilder&
{
    dynamicStates.push_back(state);
    return *this;
}

auto PipelineBuilder::addViewportScissorDynamicStates() -> PipelineBuilder&
{
    dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
    return *this;
}

auto PipelineBuilder::enableDepthBias() -> PipelineBuilder&
{
    rasterizer.depthBiasEnable = VK_TRUE;
    dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    return *this;
}