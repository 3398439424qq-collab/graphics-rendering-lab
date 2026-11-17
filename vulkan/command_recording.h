// 命令录制模块：创建图形命令池，并为每个帧缓冲预录一条命令缓冲：
// 开始 render pass（清屏）→ 绑定管线 → 画 3 个顶点 → 结束 render pass。
#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <vector>
#include <cstddef>
#include <type_traits>

struct CommandPoolAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkCommandPool pool = VK_NULL_HANDLE;
};
CommandPoolAttempt create_graphics_command_pool(
    VkDevice device, std::uint32_t queue_family_index,
    PFN_vkCreateCommandPool create_fn);

static_assert(sizeof(float) == 4, "MVP requires 32-bit float");

struct TransformPush {
    std::array<float, 16> mvp{};                        // offset 0,   64 bytes
    std::array<float, 12> normal_columns_camera_relative{};            // offset 64, 48 bytes (normal xyz + camera-relative w)
    std::array<float, 4>  light_direction_intensity{};  // offset 112, 16 bytes (xyz + intensity)
};

// Codex mechanical checks for the reviewed byte-upload contract.
static_assert(std::is_standard_layout_v<TransformPush>, "TransformPush requires standard layout");
static_assert(std::is_trivially_copyable_v<TransformPush>, "TransformPush must be byte-copyable");
static_assert(offsetof(TransformPush, mvp)                       == 0,   "TransformPush.mvp must be at offset 0");
static_assert(offsetof(TransformPush, normal_columns_camera_relative)           == 64,  "TransformPush.normal_columns_camera_relative must be at offset 64");
static_assert(offsetof(TransformPush, light_direction_intensity) == 112, "TransformPush.light_direction_intensity must be at offset 112");
static_assert(sizeof(TransformPush)                              == 128, "TransformPush must be 128 bytes");

struct MvpConfig {
    bool enabled = false;
    TransformPush push{};;
};
struct TriangleRecordingConfig {
    VkDevice device = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkExtent2D extent{};
    std::vector<VkFramebuffer> framebuffers;
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    std::uint32_t   index_count   = 0;
    VkIndexType   index_type   = VK_INDEX_TYPE_UINT16;
    // Borrowed for optional texture descriptor binding.
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorSet texture_set = VK_NULL_HANDLE;
    MvpConfig mvp{};
    VkDescriptorSet material_set = VK_NULL_HANDLE;
    bool depth_enabled = false;
};
struct CommandRecordingApi {
    PFN_vkAllocateCommandBuffers allocate = nullptr;
    PFN_vkBeginCommandBuffer begin = nullptr;
    PFN_vkCmdBeginRenderPass begin_render_pass = nullptr;
    PFN_vkCmdBindPipeline bind_pipeline = nullptr;
    PFN_vkCmdDraw draw = nullptr;
    PFN_vkCmdEndRenderPass end_render_pass = nullptr;
    PFN_vkEndCommandBuffer end = nullptr;
    PFN_vkCmdBindVertexBuffers bind_vertex_buffers = nullptr;
    PFN_vkCmdBindIndexBuffer bind_index_buffer = nullptr;
    PFN_vkCmdDrawIndexed draw_indexed = nullptr;
    PFN_vkCmdBindDescriptorSets bind_descriptor_sets = nullptr;
    PFN_vkCmdPushConstants push_constants = nullptr;
};
struct CommandRecordingAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    std::uint32_t allocated_count = 0;
    std::uint32_t recorded_count = 0;
    std::vector<VkCommandBuffer> buffers;
};
CommandRecordingAttempt record_triangle_command_buffers(
    const TriangleRecordingConfig& config, const CommandRecordingApi& api);
