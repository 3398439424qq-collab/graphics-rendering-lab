#include "graphics_pipeline.h"
#include "mesh_data.h"
#include "command_recording.h"
#include <array>
#include <cstddef>

PipelineLayoutAttempt create_empty_pipeline_layout(
    VkDevice device, PFN_vkCreatePipelineLayout create_fn){
    PipelineLayoutAttempt out;
    VkPipelineLayoutCreateInfo ci{};

    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount = 0;              // 常量
    ci.pSetLayouts = nullptr;
    ci.pushConstantRangeCount = 0;      // 常量
    ci.pPushConstantRanges = nullptr;

    out.result = create_fn(device,&ci,nullptr,&out.layout);
    if(out.result != VK_SUCCESS){
        out.layout = VK_NULL_HANDLE;
        return out;
    }
    return out;
}

// Initial scaffold prepared by Codex from the reviewed VK05 contract.
PipelineLayoutAttempt create_texture_pipeline_layout(
    VkDevice device, VkDescriptorSetLayout texture_set_layout,
    PFN_vkCreatePipelineLayout create_fn) {
    PipelineLayoutAttempt out{};
    VkPipelineLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount = 1;
    ci.pSetLayouts = &texture_set_layout;
    ci.pushConstantRangeCount = 0;
    ci.pPushConstantRanges = nullptr;
    out.result = create_fn(device, &ci, nullptr, &out.layout);
    if(out.result != VK_SUCCESS){
        out.layout = VK_NULL_HANDLE;
        return out;
    }
    return out;
}

GraphicsPipelineAttempt create_triangle_graphics_pipeline(
    const GraphicsPipelineConfig& config,
    PFN_vkCreateGraphicsPipelines create_fn){
    GraphicsPipelineAttempt out{};
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = config.vertex_module;
    stages[0].pName  = "main";
    stages[0].pSpecializationInfo = nullptr;

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = config.fragment_module;
    stages[1].pName  = "main";
    stages[1].pSpecializationInfo = nullptr;

    VkVertexInputBindingDescription binding{};
    std::array<VkVertexInputAttributeDescription, 3> attributes{};

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount   = 0;
    vertex_input.pVertexBindingDescriptions      = nullptr;
    vertex_input.vertexAttributeDescriptionCount = 0;
    vertex_input.pVertexAttributeDescriptions    = nullptr;

    switch (config.vertex_layout) {
    case VertexInputLayout::NoBuffer:
        break;

    case VertexInputLayout::Position2:
        binding.binding   = 0;
        binding.stride    = 8;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[0].binding  = binding.binding;
        attributes[0].format   = VK_FORMAT_R32G32_SFLOAT;
        attributes[0].location = 0;
        attributes[0].offset   = 0;

        vertex_input.vertexBindingDescriptionCount   = 1;
        vertex_input.pVertexBindingDescriptions      = &binding;
        vertex_input.vertexAttributeDescriptionCount = 1;
        vertex_input.pVertexAttributeDescriptions    = attributes.data();
        break;

    case VertexInputLayout::Mesh:
        binding.binding   = 0;
        binding.stride    = static_cast<std::uint32_t>(sizeof(MeshVertex));
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributes[0].binding  = binding.binding;
        attributes[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].location = 0;
        attributes[0].offset   =
            static_cast<std::uint32_t>(offsetof(MeshVertex, position));

        attributes[1].binding  = binding.binding;
        attributes[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].location = 1;
        attributes[1].offset   =
            static_cast<std::uint32_t>(offsetof(MeshVertex, normal));

        attributes[2].binding  = binding.binding;
        attributes[2].format   = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].location = 2;
        attributes[2].offset   =
            static_cast<std::uint32_t>(offsetof(MeshVertex, uv));

        vertex_input.vertexBindingDescriptionCount   = 1;
        vertex_input.pVertexBindingDescriptions      = &binding;
        vertex_input.vertexAttributeDescriptionCount = 3;
        vertex_input.pVertexAttributeDescriptions    = attributes.data();
        break;
    }

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(config.extent.width);
    viewport.height   = static_cast<float>(config.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = config.extent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports    = &viewport;
    viewport_state.scissorCount  = 1;
    viewport_state.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable        = VK_FALSE;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterization.cullMode                = VK_CULL_MODE_NONE;
    rasterization.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthBiasEnable         = VK_FALSE;
    rasterization.depthBiasConstantFactor = 0.0f;
    rasterization.depthBiasClamp          = 0.0f;
    rasterization.depthBiasSlopeFactor    = 0.0f;
    rasterization.lineWidth               = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisample.sampleShadingEnable   = VK_FALSE;
    multisample.minSampleShading      = 0.0f;
    multisample.pSampleMask           = nullptr;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable      = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable =
        config.depth_enabled ? VK_TRUE : VK_FALSE;
    depth_stencil.depthWriteEnable =
        config.depth_enabled ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp =
        config.depth_enabled ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_ALWAYS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable     = VK_FALSE;
    depth_stencil.front                 = {};
    depth_stencil.back                  = {};
    depth_stencil.minDepthBounds        = 0.0f;
    depth_stencil.maxDepthBounds        = 1.0f;

    // 颜色混合：关闭混合，直接写入 RGBA 四个通道
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.blendEnable         = VK_FALSE;
    blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
    blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.logicOpEnable   = VK_FALSE;
    color_blend.logicOp         = VK_LOGIC_OP_COPY;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments    = &blend_attachment;
    color_blend.blendConstants[0] = 0.0f;
    color_blend.blendConstants[1] = 0.0f;
    color_blend.blendConstants[2] = 0.0f;
    color_blend.blendConstants[3] = 0.0f;

    // 汇总以上各状态，创建图形管线
    VkGraphicsPipelineCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.stageCount          = 2;
    ci.pStages             = stages;
    ci.pVertexInputState   = &vertex_input;
    ci.pInputAssemblyState = &input_assembly;
    ci.pViewportState      = &viewport_state;
    ci.pRasterizationState = &rasterization;
    ci.pMultisampleState   = &multisample;
    ci.pDepthStencilState  = &depth_stencil;
    ci.pColorBlendState    = &color_blend;
    ci.pTessellationState  = nullptr;
    ci.pDynamicState       = nullptr;
    ci.layout              = config.layout;
    ci.renderPass          = config.render_pass;
    ci.subpass             = 0;
    ci.basePipelineHandle  = VK_NULL_HANDLE;
    ci.basePipelineIndex   = -1;

    out.result = create_fn(
        config.device,
        VK_NULL_HANDLE,
        1,
        &ci,
        nullptr,
        &out.pipeline);

    if (out.result != VK_SUCCESS) {
        out.pipeline = VK_NULL_HANDLE;
    }

    return out;
}

PipelineLayoutAttempt create_texture_mvp_pipeline_layout(
    VkDevice device, VkDescriptorSetLayout texture_set_layout,
    VkDescriptorSetLayout material_set_layout,
    PFN_vkCreatePipelineLayout create_fn){
    PipelineLayoutAttempt out{};
    VkPipelineLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkDescriptorSetLayout layouts[2] = {
        texture_set_layout, material_set_layout
    };

    ci.pSetLayouts = layouts;

    if (material_set_layout == VK_NULL_HANDLE) {
        ci.setLayoutCount = 1;
    } else {
        ci.setLayoutCount = 2;
    }

    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset     = 0;
    range.size       = sizeof(TransformPush);

    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges    = &range;

    out.result = create_fn(device, &ci, nullptr, &out.layout);
    if (out.result != VK_SUCCESS) {
        out.layout = VK_NULL_HANDLE;
        return out;
    }
    return out;
}