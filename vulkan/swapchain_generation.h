// 交换链代际模块：把交换链及其依赖资源（视图、render pass、帧缓冲、管线、命令、信号量）
// 作为“一代”整体构建或销毁，为窗口尺寸变化时重建交换链做准备。
#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <memory> 
#include "swapchain.h"
#include "command_recording.h"
#include "frame_presentation.h"
#include "graphics_pipeline.h"
#include "image_resources.h"

// Explicit owner of one never-used candidate generation; no automatic cleanup.
struct SwapchainGeneration {
    SwapchainGeneration() = default;
    ~SwapchainGeneration() = default;
    SwapchainGeneration(const SwapchainGeneration&) = delete;
    SwapchainGeneration& operator=(const SwapchainGeneration&) = delete;
    SwapchainGeneration(SwapchainGeneration&&) = delete;
    SwapchainGeneration& operator=(SwapchainGeneration&&) = delete;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images{}; // Borrowed from swapchain.
    std::vector<VkImageView> image_views{};
    VkRenderPass render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers{};
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers{}; // Owned by command_pool.
    std::vector<VkSemaphore> render_finished{};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    ImageAllocation depth{};
    VkImageView depth_view = VK_NULL_HANDLE;
};

struct SwapchainGenerationDestroyApi {
    PFN_vkDestroyCommandPool destroy_command_pool = nullptr;
    PFN_vkDestroyPipeline destroy_pipeline = nullptr;
    PFN_vkDestroyPipelineLayout destroy_pipeline_layout = nullptr;
    PFN_vkDestroyFramebuffer destroy_framebuffer = nullptr;
    PFN_vkDestroyRenderPass destroy_render_pass = nullptr;
    PFN_vkDestroyImageView destroy_image_view = nullptr;
    PFN_vkDestroySemaphore destroy_semaphore = nullptr;
    PFN_vkDestroySwapchainKHR destroy_swapchain = nullptr;
    ImageResourcesApi image_resources{};
};

// Valid device and callbacks; no concurrent access, acquisition, submission or
// presentation involving this candidate. Never accepts a live/retiring old generation.
// Destroys recorded owned resources, clears all fields; does not wait or create.
// 销毁一代尚未投入使用的资源，并清空所有字段。
void destroy_unused_swapchain_generation(
    VkDevice device,
    SwapchainGeneration& generation,
    const SwapchainGenerationDestroyApi& api);

// Declarations assembled by Codex from the reviewed swapchain-generation contract.
struct GenerationBuildConfig {
    SwapchainConfig swapchain{};
    std::uint32_t graphics_queue_family_index = 0;
    VkShaderModule vertex_module = VK_NULL_HANDLE;
    VkShaderModule fragment_module = VK_NULL_HANDLE;
    VertexInputLayout vertex_layout = VertexInputLayout::NoBuffer;
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    std::uint32_t   index_count   = 0;
    VkIndexType   index_type   = VK_INDEX_TYPE_UINT16;
    // Borrowed together from the parent's texture binding resources.
    VkDescriptorSetLayout texture_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet texture_set = VK_NULL_HANDLE;
    // Borrowed together from the parent's material binding resources.
    VkDescriptorSetLayout material_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet material_set = VK_NULL_HANDLE;
    MvpConfig mvp{};
    VkPhysicalDeviceMemoryProperties mem_props{};
    bool depth_enabled = false;
};
enum class GenerationBuildStage {
    Swapchain, Images, Views, RenderPass, Framebuffers, PipelineLayout,
    Pipeline, CommandPool, CommandBuffers, RenderFinished, Complete,DepthAllocation,DepthView
};
struct GenerationBuildAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    GenerationBuildStage stage = GenerationBuildStage::Swapchain;
    bool complete = false;
    bool old_retired = false;
    EnumerationStatus images_status = EnumerationStatus::CountFailed;
    ImageAllocationAttempt depth_allocation{};
    bool depth_allocation_attempted = false;
    bool result_valid = true;
};
struct GenerationBuildApi {
    PFN_vkCreateSwapchainKHR create_swapchain = nullptr;
    PFN_vkGetSwapchainImagesKHR get_images = nullptr;
    PFN_vkCreateImageView create_image_view = nullptr;
    PFN_vkCreateRenderPass create_render_pass = nullptr;
    PFN_vkCreateFramebuffer create_framebuffer = nullptr;
    PFN_vkCreatePipelineLayout create_pipeline_layout = nullptr;
    PFN_vkCreateGraphicsPipelines create_graphics_pipelines = nullptr;
    PFN_vkCreateCommandPool create_command_pool = nullptr;
    PFN_vkCreateSemaphore create_semaphore = nullptr;
    CommandRecordingApi recording{};
    ImageResourcesApi image_resources{};
};

// Empty unused candidate, valid borrowed inputs/callbacks; no wait/destroy/publication.
// Failure returns recorded successful prefix; caller cleans the unused candidate.
// Preserve full attempt, including complete/stage/images_status and old_retired.
// 按 交换链 → 图像 → 视图 → render pass → 帧缓冲 → 管线 → 命令 → 信号量 的顺序构建；首错即停，已建部分由调用者清理。
GenerationBuildAttempt build_swapchain_generation(
    const GenerationBuildConfig& config,
    SwapchainGeneration& candidate,
    const GenerationBuildApi& api);

// ==================== 首代会话与候选槽管理 ====================
struct GenerationSlot {
    SwapchainGeneration generation{};
    SwapchainPresentTracker tracker{};
};
struct SwapchainSession {
    std::unique_ptr<GenerationSlot> active_slot_{};
    std::unique_ptr<GenerationSlot> pending_slot_{};
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkFence in_flight = VK_NULL_HANDLE;
};
GenerationBuildAttempt build_candidate_generation(
    SwapchainSession& session,
    const GenerationBuildConfig& build_cfg,
    const GenerationBuildApi& build_api,
    const SwapchainGenerationDestroyApi& destroy_api);
bool publish_first_generation(SwapchainSession& session);

enum class MvpUpdateStatus { Ready, Retry, Failed };

enum class MvpUpdateStage {
    Validate, Wait, CommandPool, CommandBuffers, Published
};

struct MvpUpdateAttempt {
    MvpUpdateStatus status = MvpUpdateStatus::Failed;
    MvpUpdateStage stage = MvpUpdateStage::Validate;
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    bool result_valid = false;
    bool in_flight_waited_ok = false;
    bool gpu_fatal = false;
    std::uint32_t allocated_count = 0;
    std::uint32_t recorded_count = 0;
};

MvpUpdateAttempt update_active_generation_mvp(
    SwapchainSession& session,
    const GenerationBuildConfig& cfg,
    const MvpConfig& new_mvp,
    const GenerationBuildApi& api,
    const SwapchainGenerationDestroyApi& destroy_api,
    PFN_vkWaitForFences wait_fn);

// ==================== 已使用代正常排空与清理 ====================
enum class GenerationDrainStatus { Ready, Retry, Blocked, Failed };

// 检查并条件等待当前 active 代的 GPU 与呈现工作完成，返回是否达到 Ready 状态。
// 前置条件：session.active_slot_ != nullptr && session.pending_slot_ == nullptr；
// 违约直接返回 Failed；不执行任何销毁操作。
GenerationDrainStatus try_drain_active_generation(
    SwapchainSession& session,
    bool& gpu_in_flight_pending,
    bool gpu_fatal_latched,
    bool has_unhandled_acquire,
    VkDevice device,
    std::uint64_t timeout_ns,
    PFN_vkWaitForFences wait_fences,
    PFN_vkResetFences reset_fences);

// 前置条件：必须已通过 try_drain_active_generation 确认为 Ready，借用已停用，之后无新工作。
// 释放 active_slot_ 中的所有代内资源与 tracker fence，将 active_slot_ 置空。
// 绝不销毁 session 拥有的跨代帧同步原语（image_available / in_flight），不销毁父 device。
void destroy_drained_active_generation(
    SwapchainSession& session,
    VkDevice device,
    const SwapchainGenerationDestroyApi& destroy_api,
    PFN_vkDestroyFence destroy_fence);
