#include "swapchain_generation.h"
#include "image_views.h"
#include "render_targets.h"
#include "graphics_pipeline.h"
#include <cassert> 
#include <utility>

namespace {
void destroy_generation_resources(
    VkDevice device,
    SwapchainGeneration& generation,
    const SwapchainGenerationDestroyApi& api) 
{
    if (generation.command_pool != VK_NULL_HANDLE) {
        api.destroy_command_pool(device, generation.command_pool, nullptr);
        generation.command_pool = VK_NULL_HANDLE;
    }
    generation.command_buffers.clear();
    if (generation.pipeline != VK_NULL_HANDLE) {
        api.destroy_pipeline(device, generation.pipeline, nullptr);
        generation.pipeline = VK_NULL_HANDLE;
    }
    if (generation.pipeline_layout != VK_NULL_HANDLE) {
        api.destroy_pipeline_layout(device, generation.pipeline_layout, nullptr);
        generation.pipeline_layout = VK_NULL_HANDLE;
    }
    for (VkFramebuffer framebuffer : generation.framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            api.destroy_framebuffer(device, framebuffer, nullptr);
        }
    }
    generation.framebuffers.clear();
    if (generation.render_pass != VK_NULL_HANDLE) {
        api.destroy_render_pass(device, generation.render_pass, nullptr);
        generation.render_pass = VK_NULL_HANDLE;
    }
    for (VkImageView image_view : generation.image_views) {
        if (image_view != VK_NULL_HANDLE) {
            api.destroy_image_view(device, image_view, nullptr);
        }
    }
    generation.image_views.clear();
    if (generation.swapchain != VK_NULL_HANDLE) {
        api.destroy_swapchain(device, generation.swapchain, nullptr);
        generation.swapchain = VK_NULL_HANDLE;
    }
    generation.images.clear();
    if (generation.depth_view != VK_NULL_HANDLE) {
        api.destroy_image_view(device, generation.depth_view, nullptr);
        generation.depth_view = VK_NULL_HANDLE;
    }
    destroy_image_allocation(device, generation.depth, api.image_resources);
    for (VkSemaphore semaphore : generation.render_finished) {
        if (semaphore != VK_NULL_HANDLE) {
            api.destroy_semaphore(device, semaphore, nullptr);
        }
    }
    generation.render_finished.clear();
    generation.format = VK_FORMAT_UNDEFINED;
    generation.extent = VkExtent2D{0, 0};
}
} // namespace

void destroy_unused_swapchain_generation(
    VkDevice device,
    SwapchainGeneration& generation,
    const SwapchainGenerationDestroyApi& api) 
{
    destroy_generation_resources(device, generation, api);
}

GenerationBuildAttempt build_swapchain_generation(
    const GenerationBuildConfig& config,
    SwapchainGeneration& candidate,
    const GenerationBuildApi& api)
{
    GenerationBuildAttempt out{};

    out.stage = GenerationBuildStage::Swapchain;

    SwapchainAttempt sc_att =
        create_swapchain(config.swapchain, api.create_swapchain);

    out.old_retired = sc_att.old_retired;
    out.result      = sc_att.result;

    if (sc_att.result != VK_SUCCESS) {
        return out;
    }

    candidate.swapchain = sc_att.swapchain;
    candidate.format    = config.swapchain.surface_format.format;
    candidate.extent    = config.swapchain.extent;

    out.stage = GenerationBuildStage::Images;

    SwapchainImagesSnapshot img_snap = query_swapchain_images(
        config.swapchain.device, candidate.swapchain, api.get_images);

    out.images_status = img_snap.status;
    out.result        = img_snap.result;

    // 只有 result == VK_SUCCESS 且 status == Complete 才算成功
    if (img_snap.result != VK_SUCCESS ||
        img_snap.status != EnumerationStatus::Complete) {
        // 失败 / Empty / Incomplete：首错停止；
        // candidate.swapchain、format、extent 保留为前缀
        return out;
    }

    // 成功：移交图像句柄，确立本代实际 N
    candidate.images = std::move(img_snap.images);
    const std::size_t N = candidate.images.size();

    out.stage = GenerationBuildStage::Views;
    candidate.image_views.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
        ImageViewConfig view_cfg{};
        view_cfg.device = config.swapchain.device;
        view_cfg.image  = candidate.images[i];
        view_cfg.format = candidate.format;
        ImageViewAttempt view_att = create_image_view(view_cfg, api.create_image_view);
        if (view_att.result != VK_SUCCESS) {
            out.result = view_att.result;
            return out; // 失败槽不读，已登记的前缀保留在 candidate.image_views 中
        }
        candidate.image_views.push_back(view_att.view);
    }

    const VkFormat depth_format = VK_FORMAT_D16_UNORM;

    if (config.depth_enabled) {
        ImageAllocationConfig depth_cfg{};
        depth_cfg.extent = {candidate.extent.width, candidate.extent.height, 1};
        depth_cfg.format = depth_format;
        depth_cfg.mem_props = config.mem_props;
        depth_cfg.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depth_cfg.required_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        out.stage = GenerationBuildStage::DepthAllocation;
        out.depth_allocation_attempted = true;
        auto depth_attempt = create_image_allocation(
            config.swapchain.device, depth_cfg, candidate.depth, api.image_resources);

        out.depth_allocation = depth_attempt;
        out.result_valid = depth_attempt.api_result_valid;
        if (out.result_valid) {
            out.result = depth_attempt.result;
        }
        if (!depth_attempt.complete) {
            return out;
        }

        ImageViewConfig depth_view_cfg{};
        depth_view_cfg.device = config.swapchain.device;
        depth_view_cfg.image = candidate.depth.image;
        depth_view_cfg.format = depth_cfg.format;
        depth_view_cfg.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;

        out.stage = GenerationBuildStage::DepthView;
        auto depth_view_attempt = create_image_view(
            depth_view_cfg, api.create_image_view);
        out.result_valid = true;
        out.result = depth_view_attempt.result;
        if (depth_view_attempt.result != VK_SUCCESS) {
            return out;
        }
        candidate.depth_view = depth_view_attempt.view;
    }

    out.stage = GenerationBuildStage::RenderPass;

    ColorRenderPassConfig rp_cfg{};
    rp_cfg.device       = config.swapchain.device;
    rp_cfg.color_format = candidate.format;
    if (config.depth_enabled) {
        rp_cfg.depth_format = depth_format;
    }
    RenderPassAttempt rp_att = create_color_render_pass(rp_cfg, api.create_render_pass);
    if (rp_att.result != VK_SUCCESS) {
        out.result = rp_att.result;
        return out;
    }
    candidate.render_pass = rp_att.render_pass;

    out.stage = GenerationBuildStage::Framebuffers;

    candidate.framebuffers.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
        FramebufferConfig fb_cfg{};
        fb_cfg.device      = config.swapchain.device;
        fb_cfg.render_pass = candidate.render_pass;
        fb_cfg.color_view  = candidate.image_views[i];
        fb_cfg.extent      = candidate.extent;
        fb_cfg.depth_view = candidate.depth_view;
        FramebufferAttempt fb_att = create_color_framebuffer(fb_cfg, api.create_framebuffer);
        if (fb_att.result != VK_SUCCESS) {
            out.result = fb_att.result;
            return out; // 失败槽不读，已登记的前缀保留
        }
        candidate.framebuffers.push_back(fb_att.framebuffer);
    }
    PipelineLayoutAttempt layout_att{};
    out.stage = GenerationBuildStage::PipelineLayout;
    if(config.mvp.enabled){
        layout_att = create_texture_mvp_pipeline_layout(
            config.swapchain.device,
            config.texture_set_layout,
            config.material_set_layout,
            api.create_pipeline_layout);
    }else if (config.texture_set_layout != VK_NULL_HANDLE) {
        layout_att = create_texture_pipeline_layout(
            config.swapchain.device,
            config.texture_set_layout,
            api.create_pipeline_layout);
    } else {
        layout_att = create_empty_pipeline_layout(
            config.swapchain.device,
            api.create_pipeline_layout);
    }
    if (layout_att.result != VK_SUCCESS) {
        out.result = layout_att.result;
        return out;
    }
    candidate.pipeline_layout = layout_att.layout;

    out.stage = GenerationBuildStage::Pipeline;
    GraphicsPipelineConfig pipe_cfg{};
    pipe_cfg.device          = config.swapchain.device;
    pipe_cfg.render_pass     = candidate.render_pass;
    pipe_cfg.layout          = candidate.pipeline_layout;
    pipe_cfg.vertex_module   = config.vertex_module;   // 借用
    pipe_cfg.fragment_module = config.fragment_module; // 借用
    pipe_cfg.extent          = candidate.extent;
    pipe_cfg.vertex_layout = config.vertex_layout;
    pipe_cfg.depth_enabled = config.depth_enabled;
    GraphicsPipelineAttempt pipe_att =
        create_triangle_graphics_pipeline(pipe_cfg, api.create_graphics_pipelines);
    if (pipe_att.result != VK_SUCCESS) {
        out.result = pipe_att.result;
        return out;
    }
    candidate.pipeline = pipe_att.pipeline;
    
    out.stage = GenerationBuildStage::CommandPool;
    CommandPoolAttempt pool_att = create_graphics_command_pool(
    config.swapchain.device,
    config.graphics_queue_family_index,
    api.create_command_pool);
    if (pool_att.result != VK_SUCCESS) {
        out.result = pool_att.result;
        return out;
    }
    candidate.command_pool = pool_att.pool;

    out.stage = GenerationBuildStage::CommandBuffers;
    TriangleRecordingConfig rec_cfg{};
    rec_cfg.device       = config.swapchain.device;
    rec_cfg.pool         = candidate.command_pool;
    rec_cfg.render_pass  = candidate.render_pass;
    rec_cfg.pipeline     = candidate.pipeline;
    rec_cfg.extent       = candidate.extent;
    rec_cfg.framebuffers = candidate.framebuffers;
    rec_cfg.vertex_buffer = config.vertex_buffer;
    rec_cfg.index_buffer = config.index_buffer;
    rec_cfg.index_count  = config.index_count;
    rec_cfg.index_type   = config.index_type;
    rec_cfg.pipeline_layout = candidate.pipeline_layout;
    rec_cfg.texture_set = config.texture_set;
    rec_cfg.material_set = config.material_set;
    rec_cfg.mvp = config.mvp;
    rec_cfg.depth_enabled = config.depth_enabled;
    CommandRecordingAttempt rec_att =
        record_triangle_command_buffers(rec_cfg, api.recording);
    if (rec_att.result != VK_SUCCESS) {
        out.result = rec_att.result;
        // 录制失败时 rec_att.buffers 为空，但 candidate.command_pool 必须保留！
        return out;
    }
    candidate.command_buffers = std::move(rec_att.buffers);

    out.stage = GenerationBuildStage::RenderFinished;
    VkSemaphoreCreateInfo sem_ci{};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    sem_ci.pNext = nullptr;
    sem_ci.flags = 0;
    candidate.render_finished.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
        VkSemaphore sem = VK_NULL_HANDLE;
        VkResult sem_res = api.create_semaphore(
            config.swapchain.device, &sem_ci, nullptr, &sem);
        if (sem_res != VK_SUCCESS) {
            out.result = sem_res;
            return out; // 失败槽不读，已 push 的前缀保留在 candidate.render_finished
        }
        candidate.render_finished.push_back(sem);
    }

    out.stage    = GenerationBuildStage::Complete;
    out.result   = VK_SUCCESS;
    out.complete = true;
    return out;
}

namespace {
    struct CandidateGuard {
        VkDevice device;
        SwapchainSession& session;
        const SwapchainGenerationDestroyApi& destroy_api;
        bool dismissed = false;
        ~CandidateGuard() noexcept {
            if (!dismissed && session.pending_slot_) {
                destroy_unused_swapchain_generation(
                    device, session.pending_slot_->generation, destroy_api);
                session.pending_slot_.reset();
            }
        }
        void dismiss() noexcept { dismissed = true; }
    };

    // 临时命令池 B 的局部守卫：失败路径析构销毁，成功发布后 dismiss 放弃所有权。
    struct MvpPoolGuard {
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool pool = VK_NULL_HANDLE;
        PFN_vkDestroyCommandPool destroy_fn = nullptr;

        MvpPoolGuard() = default;
        ~MvpPoolGuard() noexcept {
            if (pool != VK_NULL_HANDLE) {
                destroy_fn(device, pool, nullptr);
            }
        }

        MvpPoolGuard(const MvpPoolGuard&) = delete;
        MvpPoolGuard& operator=(const MvpPoolGuard&) = delete;
        MvpPoolGuard(MvpPoolGuard&&) = delete;
        MvpPoolGuard& operator=(MvpPoolGuard&&) = delete;

        void dismiss() noexcept {
            pool = VK_NULL_HANDLE;
        }
    };
}

GenerationBuildAttempt build_candidate_generation(
    SwapchainSession& session,
    const GenerationBuildConfig& build_cfg,
    const GenerationBuildApi& build_api,
    const SwapchainGenerationDestroyApi& destroy_api){
    GenerationBuildAttempt attempt{};

    if (build_cfg.swapchain.device == VK_NULL_HANDLE) {
        attempt.result   = VK_ERROR_INITIALIZATION_FAILED;
        attempt.stage    = GenerationBuildStage::Swapchain;
        attempt.complete = false;
        return attempt;
    }

    CandidateGuard guard{
        build_cfg.swapchain.device,
        session,
        destroy_api,
        false
    };
    
    session.pending_slot_ = std::make_unique<GenerationSlot>();
    attempt = build_swapchain_generation(build_cfg, session.pending_slot_->generation, build_api);
    if(!attempt.complete){
        return attempt;
    }
    guard.dismiss();
    return attempt;
}

bool publish_first_generation(SwapchainSession& session){
    // 1. Debug 诊断：在 Debug 编译下，若 active 已存在，直接中断报错定位
    assert(!session.active_slot_ && "active_slot_ must be null for first generation publish");
    if (session.active_slot_ != nullptr) {
        return false;
    }
    if (session.pending_slot_ == nullptr) {
        return false;
    }
    session.active_slot_ = std::move(session.pending_slot_);
    return true;
}

MvpUpdateAttempt update_active_generation_mvp(
    SwapchainSession& session,
    const GenerationBuildConfig& cfg,
    const MvpConfig& new_mvp,
    const GenerationBuildApi& api,
    const SwapchainGenerationDestroyApi& destroy_api,
    PFN_vkWaitForFences wait_fn){
    MvpUpdateAttempt out{};
    out.status = MvpUpdateStatus::Failed;
    out.stage = MvpUpdateStage::Validate;
    out.result_valid = false;
    if (session.active_slot_ == nullptr ||
    session.pending_slot_ != nullptr) {
    return out; // Failed @ Validate
    }
    if (session.in_flight == VK_NULL_HANDLE ||
        wait_fn == nullptr) {
        return out;
    }

    if (cfg.swapchain.device == VK_NULL_HANDLE) {
        return out;
    }

    const SwapchainGeneration& active_probe = session.active_slot_->generation;
    if (active_probe.pipeline_layout == VK_NULL_HANDLE ||
        active_probe.pipeline == VK_NULL_HANDLE ||
        active_probe.render_pass == VK_NULL_HANDLE ||
        active_probe.command_pool == VK_NULL_HANDLE ||
        active_probe.framebuffers.empty() ||
        active_probe.command_buffers.empty() ||
        active_probe.extent.width == 0 ||
        active_probe.extent.height == 0) {
        return out;
    }
        // 创建 B 和失败清理都必须有对应回调。
    if (api.create_command_pool == nullptr ||
        destroy_api.destroy_command_pool == nullptr) {
        return out;
    }

    // 给录制回调表取一个只读引用短名。
    const auto& rec_api = api.recording;

    // 每次录制都会调用这些回调。
    if (rec_api.allocate == nullptr ||
        rec_api.begin == nullptr ||
        rec_api.begin_render_pass == nullptr ||
        rec_api.bind_pipeline == nullptr ||
        rec_api.end_render_pass == nullptr ||
        rec_api.end == nullptr) {
        return out;
    }

    // 启用 MVP，才会写 push constants。
    if (new_mvp.enabled &&
        rec_api.push_constants == nullptr) {
        return out;
    }

    // 有纹理集，才会绑定描述符集。
    if ((cfg.texture_set != VK_NULL_HANDLE ||
        cfg.material_set != VK_NULL_HANDLE) &&
        rec_api.bind_descriptor_sets == nullptr) {
        return out;
    }

    // 有顶点缓冲，才会绑定顶点缓冲。
    if (cfg.vertex_buffer != VK_NULL_HANDLE &&
        rec_api.bind_vertex_buffers == nullptr) {
        return out;
    }

    // 根据是否有索引缓冲，选择绘制路径。
    if (cfg.index_buffer != VK_NULL_HANDLE) {
        if (rec_api.bind_index_buffer == nullptr ||
            rec_api.draw_indexed == nullptr) {
            return out;
        }
    } else if (rec_api.draw == nullptr) {
        return out;
    }

    if (new_mvp.enabled != cfg.mvp.enabled) {
        return out;
    }

    SwapchainGeneration& active = session.active_slot_->generation;

    out.stage = MvpUpdateStage::Wait;
    out.result = wait_fn(cfg.swapchain.device, 1, &session.in_flight, VK_TRUE, 1000000000ULL);
    out.result_valid = true;
    if(out.result == VK_TIMEOUT){
        out.status = MvpUpdateStatus::Retry;
        return out;
    }else if(out.result != VK_SUCCESS){
        out.gpu_fatal = true;
        return out;
    }
    out.in_flight_waited_ok = true;

    out.stage = MvpUpdateStage::CommandPool;
    MvpPoolGuard guard{};
    const VkDevice device = cfg.swapchain.device;
    guard.device = device;
    guard.destroy_fn = destroy_api.destroy_command_pool;

    CommandPoolAttempt pool_att{};
    pool_att = create_graphics_command_pool(
        device, cfg.graphics_queue_family_index, api.create_command_pool);

    out.result = pool_att.result;
    out.result_valid = true;

    if (pool_att.result != VK_SUCCESS) {
        out.gpu_fatal = (pool_att.result == VK_ERROR_DEVICE_LOST);
        return out;   // B 未产生，guard.pool 仍空，析构跳过；A 保留
    }
    guard.pool = pool_att.pool;  

    out.stage = MvpUpdateStage::CommandBuffers;
    TriangleRecordingConfig rec_cfg{};
    rec_cfg.depth_enabled   = cfg.depth_enabled;
    rec_cfg.device          = device;
    rec_cfg.pool            = guard.pool;
    rec_cfg.extent          = active.extent;
    rec_cfg.framebuffers    = active.framebuffers;
    rec_cfg.pipeline_layout = active.pipeline_layout;
    rec_cfg.pipeline        = active.pipeline;
    rec_cfg.render_pass     = active.render_pass;
    rec_cfg.vertex_buffer   = cfg.vertex_buffer;
    rec_cfg.index_buffer    = cfg.index_buffer;
    rec_cfg.texture_set     = cfg.texture_set;
    rec_cfg.material_set = cfg.material_set;
    rec_cfg.index_count     = cfg.index_count;
    rec_cfg.index_type      = cfg.index_type;
    rec_cfg.mvp             = new_mvp;

    CommandRecordingAttempt rec_att = record_triangle_command_buffers(rec_cfg, api.recording);
    out.result = rec_att.result;
    out.result_valid = true;
    out.allocated_count = rec_att.allocated_count;
    out.recorded_count  = rec_att.recorded_count;
    if (rec_att.result != VK_SUCCESS) {
        out.gpu_fatal = (rec_att.result == VK_ERROR_DEVICE_LOST);
        return out; 
    }
    if (out.allocated_count != active.framebuffers.size()||
        out.recorded_count != active.framebuffers.size() ||
        rec_att.buffers.size() != active.framebuffers.size()){
        return out;
    }
    const VkCommandPool old_pool = active.command_pool;
    active.command_pool = guard.pool;
    active.command_buffers.swap(rec_att.buffers);
    guard.dismiss();
    if (old_pool != VK_NULL_HANDLE) {
       destroy_api.destroy_command_pool(device, old_pool, nullptr);
    }
    
    out.stage = MvpUpdateStage::Published;
    out.status = MvpUpdateStatus::Ready;
    out.result       = VK_SUCCESS;
    out.result_valid = true;
    return out;
}

GenerationDrainStatus try_drain_active_generation(
    SwapchainSession& session,
    bool& gpu_in_flight_pending,
    bool gpu_fatal_latched,
    bool has_unhandled_acquire,
    VkDevice device,
    std::uint64_t timeout_ns,
    PFN_vkWaitForFences wait_fences,
    PFN_vkResetFences reset_fences) 
{
    // 1. 槽前置硬约束（active 必须存在且 pending 为空）
    if (session.active_slot_ == nullptr || session.pending_slot_ != nullptr) {
        return GenerationDrainStatus::Failed;
    }
    // 2. 异常与阻断门控
    if (gpu_fatal_latched) {
        return GenerationDrainStatus::Failed;
    }
    if (has_unhandled_acquire) {
        return GenerationDrainStatus::Blocked;
    }
    auto& tracker = session.active_slot_->tracker;
    if (tracker.status == PresentTrackingState::Failed) {
        return GenerationDrainStatus::Failed;
    }
    // 3. 条件等待 GPU 侧（in-flight fence）
    if (gpu_in_flight_pending) {
        if (wait_fences == nullptr) {
            return GenerationDrainStatus::Failed;
        }
        VkResult wait_res = wait_fences(device, 1, &session.in_flight, VK_TRUE, timeout_ns);
        if (wait_res == VK_SUCCESS) {
            gpu_in_flight_pending = false; // 成功清除挂起事实，不重置 GPU fence
        } else if (wait_res == VK_TIMEOUT) {
            return GenerationDrainStatus::Retry; // 超时保持 pending
        } else {
            return GenerationDrainStatus::Failed;
        }
    }
    // 4. 条件等待呈现侧（present fence）
    if (tracker.status == PresentTrackingState::Pending) {
        PresentWaitOutcome outcome = swapchain_present_tracker_wait_and_rearm(
            tracker, device, timeout_ns, wait_fences, reset_fences);
        if (outcome == PresentWaitOutcome::Retry) {
            return GenerationDrainStatus::Retry;
        } else if (outcome == PresentWaitOutcome::Failed) {
            return GenerationDrainStatus::Failed;
        }
    }
    // 5. 终态核验（仅全部完成且无阻断报 Ready）
    if (!gpu_in_flight_pending &&
        tracker.status == PresentTrackingState::Idle &&
        !gpu_fatal_latched &&
        !has_unhandled_acquire) 
    {
        return GenerationDrainStatus::Ready;
    }
    return GenerationDrainStatus::Failed;
}

void destroy_drained_active_generation(
    SwapchainSession& session,
    VkDevice device,
    const SwapchainGenerationDestroyApi& destroy_api,
    PFN_vkDestroyFence destroy_fence) 
{
    if (session.active_slot_ == nullptr) {
        return;
    }
    // 1. 释放代内独占资源
    destroy_generation_resources(device, session.active_slot_->generation, destroy_api);
    // 2. 销毁代内 tracker 持有的 present_fence
    if (destroy_fence != nullptr) {
        swapchain_present_tracker_destroy(session.active_slot_->tracker, device, destroy_fence);
    }
    // 3. 置空 active 槽（跨代同步原语与父 device 保留）
    session.active_slot_.reset();
}