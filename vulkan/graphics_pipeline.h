// 图形管线模块：创建空管线布局，以及画三角形用的图形管线（无顶点输入、固定视口、不混合、不做深度测试）。
#pragma once
#include <vulkan/vulkan.h>

struct PipelineLayoutAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkPipelineLayout layout = VK_NULL_HANDLE;
};
PipelineLayoutAttempt create_empty_pipeline_layout(
    VkDevice device, PFN_vkCreatePipelineLayout create_fn);

// Borrows one texture set layout; the returned pipeline layout belongs to the caller.
PipelineLayoutAttempt create_texture_pipeline_layout(
    VkDevice device, VkDescriptorSetLayout texture_set_layout,
    PFN_vkCreatePipelineLayout create_fn);

// Reviewed VK07 layout selector; contains no resource ownership.
enum class VertexInputLayout {
    NoBuffer,
    Position2,
    Mesh
};

struct GraphicsPipelineConfig {
    VkDevice device = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule vertex_module = VK_NULL_HANDLE;
    VkShaderModule fragment_module = VK_NULL_HANDLE;
    VkExtent2D extent{};
    VertexInputLayout vertex_layout = VertexInputLayout::NoBuffer;
    bool depth_enabled = false;
};
struct GraphicsPipelineAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkPipeline pipeline = VK_NULL_HANDLE;
};
GraphicsPipelineAttempt create_triangle_graphics_pipeline(
    const GraphicsPipelineConfig& config,
    PFN_vkCreateGraphicsPipelines create_fn);

PipelineLayoutAttempt create_texture_mvp_pipeline_layout(
    VkDevice device, VkDescriptorSetLayout texture_set_layout,
    VkDescriptorSetLayout material_set_layout,
    PFN_vkCreatePipelineLayout create_fn);