// 帧呈现模块：创建帧同步对象（信号量与栅栏），并执行一帧“等待 → 获取图像 → 重置栅栏 → 提交 → 呈现”，
// 把结果归类为 已呈现 / 重试 / 交换链需重建 / 失败。
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

enum class FrameResult { Presented, Retry, SwapchainChanged, Failed };
enum class FrameStage { Wait, Acquire, Reset, Submit, Present };
struct FrameSyncAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkSemaphore image_available = VK_NULL_HANDLE;
    std::vector<VkSemaphore> render_finished;
    VkFence in_flight = VK_NULL_HANDLE;
};
FrameSyncAttempt create_frame_sync(
    VkDevice device, std::uint32_t image_count,
    PFN_vkCreateSemaphore create_semaphore, PFN_vkCreateFence create_fence);

struct FrameConfig {
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkFence present_fence = VK_NULL_HANDLE;
    const std::vector<VkSemaphore>* render_finished = nullptr;
    VkFence in_flight = VK_NULL_HANDLE;
    const std::vector<VkCommandBuffer>* command_buffers = nullptr;
    
};
struct FrameApi {
    PFN_vkWaitForFences wait = nullptr;
    PFN_vkAcquireNextImageKHR acquire = nullptr;
    PFN_vkResetFences reset = nullptr;
    PFN_vkQueueSubmit submit = nullptr;
    PFN_vkQueuePresentKHR present = nullptr;
};
struct FrameDrawAttempt {
    FrameResult status = FrameResult::Failed;
    FrameStage stage = FrameStage::Wait;
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    std::uint32_t image_index = UINT32_MAX;
    bool submitted = false; // 表示 submit 已被调用，不代表提交成功或执行完成。
    bool present_called = false;
    bool submit_success = false;                             // 仅当 api.submit 成功返回 VK_SUCCESS 时置 true
    VkResult present_result = VK_ERROR_INITIALIZATION_FAILED; // 真实记录 vkQueuePresentKHR 的底层返回值
    bool in_flight_waited_ok = false;                        // 仅当开头 api.wait 成功后、reset 之前置 true
    bool gpu_fatal = false;                                  // 遇到不可恢复故障（DEVICE_LOST、非预期API失败等）时置 
};
FrameDrawAttempt draw_triangle_frame(const FrameConfig& config, const FrameApi& api);

enum class PresentTrackingState {
    Idle,     // 确认无挂起，可跳过等待 / 满足回收前置
    Pending,  // 呈现已入队，必须等待其 fence 信号化
    Failed    // 严重未知错误，锁存失败，严禁冒充安全排空
};
enum class PresentWaitOutcome {
    Ok,      // 无需等待，或等待成功并已 reset，可继续渲染
    Retry,   // 等待超时，fence 仍未信号，本帧应放弃并重试
    Failed   // 等待失败 / 设备丢失，Tracker 已置为 Failed
};

struct SwapchainPresentTracker {
    VkFence fence = VK_NULL_HANDLE;
    PresentTrackingState status = PresentTrackingState::Idle;
    VkResult last_present_result = VK_NOT_READY;
};
VkResult swapchain_present_tracker_init(
    SwapchainPresentTracker& tracker,
    VkDevice device,
    PFN_vkCreateFence create_fence);
PresentWaitOutcome swapchain_present_tracker_wait_and_rearm(
    SwapchainPresentTracker& tracker,
    VkDevice device,
    std::uint64_t timeout_ns,
    PFN_vkWaitForFences wait_fences,
    PFN_vkResetFences reset_fences);
void swapchain_present_tracker_on_presented(
    SwapchainPresentTracker& tracker,
    VkResult present_result,
    bool did_present);
bool swapchain_present_tracker_can_retire(
    const SwapchainPresentTracker& tracker);
VkResult swapchain_present_tracker_destroy(
    SwapchainPresentTracker& tracker,
    VkDevice device,
    PFN_vkDestroyFence destroy_fence);

// ==================== 专用帧级同步原语创建与清理 ====================
VkResult create_frame_sync_primitives(
    VkDevice device,
    VkSemaphore* out_image_available,
    VkFence* out_in_flight,
    PFN_vkCreateSemaphore create_semaphore,
    PFN_vkCreateFence create_fence,
    PFN_vkDestroySemaphore destroy_semaphore);
void destroy_frame_sync_primitives(
    VkDevice device,
    VkSemaphore& image_available,
    VkFence& in_flight,
    PFN_vkDestroySemaphore destroy_semaphore,
    PFN_vkDestroyFence destroy_fence);

enum class ExitConsumeStage { NotSubmitted, SubmittedWaiting, Consumed, Failed };
enum class ExitConsumeFailedStage { None, SubmitFailed, WaitFailed };
enum class ExitConsumeStatus { Retry, Done, Error };

struct ExitConsumeState {
    ExitConsumeStage stage = ExitConsumeStage::NotSubmitted;
    ExitConsumeFailedStage failed_stage = ExitConsumeFailedStage::None;
    VkResult last_vk_result = VK_SUCCESS;
};

ExitConsumeStatus try_consume_acquire_for_exit(
    VkDevice device,
    VkQueue queue,
    VkSemaphore image_available,
    VkFence in_flight,
    bool& gpu_in_flight_pending,
    bool& gpu_fatal_latched,
    PFN_vkQueueSubmit pfn_vkQueueSubmit,
    PFN_vkWaitForFences pfn_vkWaitForFences,
    uint64_t wait_timeout_ns,
    ExitConsumeState* state,
    VkResult* out_vk_result);