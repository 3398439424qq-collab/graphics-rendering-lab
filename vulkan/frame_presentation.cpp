#include "frame_presentation.h"

FrameSyncAttempt create_frame_sync(
    VkDevice device, std::uint32_t image_count,
    PFN_vkCreateSemaphore create_semaphore, PFN_vkCreateFence create_fence) {
    FrameSyncAttempt out{};

    VkSemaphoreCreateInfo sem_ci{};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    sem_ci.pNext = nullptr;
    sem_ci.flags = 0;

    VkSemaphore image_available = VK_NULL_HANDLE;   // 失败槽不读，先置空
    out.result = create_semaphore(device, &sem_ci, nullptr, &image_available);
    if (out.result != VK_SUCCESS) {
        return out;   // 失败：前缀为空，不 destroy、不重试
    }
    out.image_available = image_available;

    out.render_finished.reserve(static_cast<std::size_t>(image_count));
    for (std::uint32_t i = 0; i < image_count; i++){
        VkSemaphore render_finished = VK_NULL_HANDLE;
        out.result = create_semaphore(device, &sem_ci, nullptr, &render_finished);
        if (out.result != VK_SUCCESS) {
            return out;   // 失败：已 push 的成功前缀保留，失败槽不读
        }
        out.render_finished.push_back(render_finished);
    }

    VkFenceCreateInfo fence_ci{};
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    fence_ci.pNext = nullptr;
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence in_flight = VK_NULL_HANDLE;
    out.result = create_fence(device, &fence_ci, nullptr, &in_flight);
    if (out.result != VK_SUCCESS) {
        return out;   // 失败：image_available + 全部 render_finished 前缀保留
    }
    out.in_flight = in_flight;

    out.result = VK_SUCCESS;
    return out;
}

FrameDrawAttempt draw_triangle_frame(const FrameConfig& config, const FrameApi& api) {
    FrameDrawAttempt out{};

    out.stage = FrameStage::Wait;

    out.result = api.wait(
        config.device, 1, &config.in_flight, VK_TRUE, 1000000000ULL);

    if (out.result == VK_TIMEOUT) {
        out.status = FrameResult::Retry;
        return out; // 保持 has_in_flight_gpu_work == true
    }

    if (out.result != VK_SUCCESS) {
        out.gpu_fatal = true;
        out.status = FrameResult::Failed;
        return out;
    }

    // 关键：wait 成功事实必须传回 main，且必须在 reset 之前置位
    out.in_flight_waited_ok = true;

    out.stage = FrameStage::Acquire;
    std::uint32_t index = UINT32_MAX;
    out.result = api.acquire(config.device, config.swapchain,
                             1000000000ULL, config.image_available,
                             VK_NULL_HANDLE, &index);
    if (out.result == VK_SUCCESS || out.result == VK_SUBOPTIMAL_KHR){
        out.image_index = index;
    } else {
        out.image_index = UINT32_MAX;
        if (out.result == VK_TIMEOUT || out.result == VK_NOT_READY) {
            out.status = FrameResult::Retry;
        } else if (out.result == VK_ERROR_OUT_OF_DATE_KHR){
            out.status = FrameResult::SwapchainChanged;
        } else if (out.result == VK_ERROR_SURFACE_LOST_KHR){
            out.status = FrameResult::Failed;
        } else {
            out.status = FrameResult::Failed;
            out.gpu_fatal = true;
        }
        return out;
    }
    const bool acquire_suboptimal = (out.result == VK_SUBOPTIMAL_KHR);
    out.stage  = FrameStage::Reset;
    out.result = api.reset(config.device, 1, &config.in_flight);
    if (out.result != VK_SUCCESS) {
        out.gpu_fatal = true;
        out.status = FrameResult::Failed;
        return out;
    }

    out.stage = FrameStage::Submit;

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;


    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext                = nullptr;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &config.image_available;
    submit.pWaitDstStageMask    = &wait_stage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &(*config.command_buffers)[index];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &(*config.render_finished)[index];

    out.submitted = true;   // 调用之前置 true：返回失败也算调用过
    out.result = api.submit(config.queue, 1, &submit, config.in_flight);
    if (out.result != VK_SUCCESS) {
        out.submit_success = false;

        // DEVICE_LOST 或未知严重错误锁存；OOM 可视为未入队，不锁存 fatal
        if (out.result != VK_ERROR_OUT_OF_HOST_MEMORY &&
            out.result != VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            out.gpu_fatal = true;
        }

        out.status = FrameResult::Failed;
        return out;
    }

    out.submit_success = true;

    out.stage = FrameStage::Present;

    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext              = nullptr;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &(*config.render_finished)[index];
    present.swapchainCount     = 1;
    present.pSwapchains        = &config.swapchain;
    present.pImageIndices      = &index;
    present.pResults           = nullptr;

    VkSwapchainPresentFenceInfoEXT present_fence_info{};
    if (config.present_fence != VK_NULL_HANDLE) {
        present_fence_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
        present_fence_info.pNext = nullptr;
        present_fence_info.swapchainCount = 1;
        present_fence_info.pFences = &config.present_fence;
        present.pNext = &present_fence_info;
    }

    out.present_called = true;
    out.result = api.present(config.queue, &present);
    out.present_result = out.result;

    if (out.result == VK_SUCCESS) {
        if (acquire_suboptimal) {
            // 已成功呈现，但 acquire 曾返回 SUBOPTIMAL：上报 SwapchainChanged，
            // 并保留 acquire 阶段与返回码，提示调用者重建交换链。
            out.status = FrameResult::SwapchainChanged;
            out.stage  = FrameStage::Acquire;
            out.result = VK_SUBOPTIMAL_KHR;
        } else {
            out.status = FrameResult::Presented;
        }
        return out;
    }

    if (out.result == VK_SUBOPTIMAL_KHR ||
        out.result == VK_ERROR_OUT_OF_DATE_KHR) {
        out.status = FrameResult::SwapchainChanged;   // 阶段留在 Present
    } else {
        out.status = FrameResult::Failed;             // 其他硬失败
    }
    return out;
}

// 1. swapchain_present_tracker_init
VkResult swapchain_present_tracker_init(
    SwapchainPresentTracker& tracker,
    VkDevice device,
    PFN_vkCreateFence create_fence)
{
    tracker.fence = VK_NULL_HANDLE;

    VkFenceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    ci.flags = 0; // 必须 unsignaled，不能带 SIGNALED_BIT

    VkFence fence = VK_NULL_HANDLE;
    VkResult r = create_fence(device, &ci, nullptr, &fence);
    if (r != VK_SUCCESS) {
        // 不读未初始化句柄；tracker.fence 保持 VK_NULL_HANDLE
        return r;
    }

    tracker.fence = fence;
    tracker.status = PresentTrackingState::Idle;
    tracker.last_present_result = VK_NOT_READY;
    return VK_SUCCESS;
}

// 2. swapchain_present_tracker_wait_and_rearm
PresentWaitOutcome swapchain_present_tracker_wait_and_rearm(
    SwapchainPresentTracker& tracker,
    VkDevice device,
    std::uint64_t timeout_ns,
    PFN_vkWaitForFences wait_fences,
    PFN_vkResetFences reset_fences)
{
    if (tracker.status == PresentTrackingState::Idle) {
        return PresentWaitOutcome::Ok;
    }
    if (tracker.status == PresentTrackingState::Failed) {
        return PresentWaitOutcome::Failed;
    }

    // 此时严格为 Pending
    VkResult wr = wait_fences(
        device, 1, &tracker.fence, VK_TRUE, timeout_ns);

    if (wr == VK_TIMEOUT) {
        // 保持 Pending 不变
        return PresentWaitOutcome::Retry;
    }
    if (wr != VK_SUCCESS) {
        tracker.status = PresentTrackingState::Failed;
        return PresentWaitOutcome::Failed;
    }

    VkResult rr = reset_fences(device, 1, &tracker.fence);
    if (rr != VK_SUCCESS) {
        tracker.status = PresentTrackingState::Failed;
        return PresentWaitOutcome::Failed;
    }

    tracker.status = PresentTrackingState::Idle;
    return PresentWaitOutcome::Ok;
}

// 3. swapchain_present_tracker_on_presented
void swapchain_present_tracker_on_presented(
    SwapchainPresentTracker& tracker,
    VkResult present_result,
    bool did_present)
{
    if (tracker.status == PresentTrackingState::Failed) {
        return; // 故障已锁存，严禁解除
    }
    if (!did_present) {
        return; // 呈现未发起，保留已有状态与事实
    }

    tracker.last_present_result = present_result;

    switch (present_result) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_ERROR_SURFACE_LOST_KHR:
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        tracker.status = PresentTrackingState::Pending;
        break;

    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        tracker.status = PresentTrackingState::Idle;
        break;

    case VK_ERROR_DEVICE_LOST:
    default:
        tracker.status = PresentTrackingState::Failed;
        break;
    }
}

// 4. swapchain_present_tracker_can_retire
bool swapchain_present_tracker_can_retire(
    const SwapchainPresentTracker& tracker)
{
    return tracker.status == PresentTrackingState::Idle;
}

// 5. swapchain_present_tracker_destroy
VkResult swapchain_present_tracker_destroy(
    SwapchainPresentTracker& tracker,
    VkDevice device,
    PFN_vkDestroyFence destroy_fence)
{
    if (tracker.status != PresentTrackingState::Idle) {
        return VK_ERROR_UNKNOWN; // 拒绝普通销毁，状态不变
    }

    if (tracker.fence != VK_NULL_HANDLE) {
        destroy_fence(device, tracker.fence, nullptr);
        tracker.fence = VK_NULL_HANDLE;
    }

    return VK_SUCCESS;
}

VkResult create_frame_sync_primitives(
    VkDevice device,
    VkSemaphore* out_image_available,
    VkFence* out_in_flight,
    PFN_vkCreateSemaphore create_semaphore,
    PFN_vkCreateFence create_fence,
    PFN_vkDestroySemaphore destroy_semaphore){
    if (out_image_available == nullptr || out_in_flight == nullptr) {
        return VK_ERROR_INITIALIZATION_FAILED;   // 出参不可写，无法回填 NULL
    }
    // 2) 出参指针有效，立即重置为 NULL_HANDLE（给调用方"全有或全无"的初值保证）
    *out_image_available = VK_NULL_HANDLE;
    *out_in_flight       = VK_NULL_HANDLE;

    // 3) 再检查 device 与三个 PFN（注入式函数指针，便于测试/免链接依赖）
    if (device                == VK_NULL_HANDLE ||
        create_semaphore  == nullptr ||
        create_fence      == nullptr ||
        destroy_semaphore == nullptr) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    sci.flags = 0;
    VkSemaphore sem = VK_NULL_HANDLE;
    VkResult result =  create_semaphore(device, &sci, nullptr, &sem);
    if(result != VK_SUCCESS){
        return result;
    }

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; 
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkFence fence = VK_NULL_HANDLE;
    result = create_fence(device, &fci, nullptr, &fence);
    if(result != VK_SUCCESS){
        destroy_semaphore(device, sem, nullptr);
        return result;
    }

    *out_image_available = sem;
    *out_in_flight = fence;
    return VK_SUCCESS;
}

void destroy_frame_sync_primitives(
    VkDevice device,
    VkSemaphore& image_available,
    VkFence& in_flight,
    PFN_vkDestroySemaphore destroy_semaphore,
    PFN_vkDestroyFence destroy_fence){
    if (device == VK_NULL_HANDLE) return;
    if (image_available != VK_NULL_HANDLE && destroy_semaphore != nullptr) {
        destroy_semaphore(device,image_available,nullptr);
        image_available = VK_NULL_HANDLE;
    }
    if (in_flight != VK_NULL_HANDLE && destroy_fence != nullptr) {
        destroy_fence(device,in_flight,nullptr);
        in_flight = VK_NULL_HANDLE;
    }
}

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
    VkResult* out_vk_result)
{
    if (state == nullptr) {
        if (out_vk_result != nullptr) {
            *out_vk_result = VK_ERROR_INITIALIZATION_FAILED;
        }
        return ExitConsumeStatus::Error;
    }

    // 阶段 0：终态回访，零 API 调用，不改写持久事实
    if (state->stage == ExitConsumeStage::Consumed) {
        if (out_vk_result != nullptr) {
            *out_vk_result = state->last_vk_result;
        }
        return ExitConsumeStatus::Done;
    }

    if (state->stage == ExitConsumeStage::Failed) {
        if (out_vk_result != nullptr) {
            *out_vk_result = state->last_vk_result;
        }
        return ExitConsumeStatus::Error;
    }

    // 阶段 1：未提交状态，提交空 batch 消费 image_available
    if (state->stage == ExitConsumeStage::NotSubmitted) {
        VkPipelineStageFlags wait_stage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = nullptr;

        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &image_available;
        submit_info.pWaitDstStageMask = &wait_stage;

        submit_info.commandBufferCount = 0;
        submit_info.pCommandBuffers = nullptr;

        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = nullptr;

        VkResult res = pfn_vkQueueSubmit(
            queue, 1, &submit_info, in_flight);

        state->last_vk_result = res;
        if (out_vk_result != nullptr) {
            *out_vk_result = res;
        }

        if (res != VK_SUCCESS) {
            state->stage = ExitConsumeStage::Failed;
            state->failed_stage = ExitConsumeFailedStage::SubmitFailed;

            // OOM 视为未入队，不锁存 fatal；其他错误锁存
            if (res != VK_ERROR_OUT_OF_HOST_MEMORY &&
                res != VK_ERROR_OUT_OF_DEVICE_MEMORY) {
                gpu_fatal_latched = true;
            }

            return ExitConsumeStatus::Error;
        }

        // 提交成功：in_flight 已绑定到该空 batch
        gpu_in_flight_pending = true;
        state->stage = ExitConsumeStage::SubmittedWaiting;

        // fallthrough 到阶段 2 等待
    }

    // 阶段 2：已提交等待状态
    // 此时 state->stage 应为 SubmittedWaiting
    VkResult res = pfn_vkWaitForFences(
        device, 1, &in_flight, VK_TRUE, wait_timeout_ns);

    state->last_vk_result = res;
    if (out_vk_result != nullptr) {
        *out_vk_result = res;
    }

    if (res == VK_SUCCESS) {
        gpu_in_flight_pending = false;
        state->stage = ExitConsumeStage::Consumed;
        return ExitConsumeStatus::Done;
    }

    if (res == VK_TIMEOUT) {
        // 保持 SubmittedWaiting，gpu_in_flight_pending 保持 true
        return ExitConsumeStatus::Retry;
    }

    // 等待硬错误，例如 DEVICE_LOST
    gpu_fatal_latched = true;

    // gpu_in_flight_pending 坚决保持 true，不清除
    state->stage = ExitConsumeStage::Failed;
    state->failed_stage = ExitConsumeFailedStage::WaitFailed;
    return ExitConsumeStatus::Error;
}