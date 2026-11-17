// Initial scaffold prepared by Codex; implementation follows the reviewed vertex-upload contract.
#include "buffer_upload.h"
#include "command_recording.h"

namespace {
// Mechanical result mapping supplied by Codex, not student behavior code.
UploadAttempt snapshot(const VertexUploadContext& ctx) {
    UploadAttempt out{};
    out.state = ctx.state;
    out.stage = ctx.stage;
    out.api_result_valid = ctx.last_result_valid;
    out.result = ctx.last_result;
    out.submit_attempted = ctx.submit_attempted;
    out.submit_success = ctx.submit_success;
    return out;
}
}

UploadAttempt prepare_vertex_upload(
    const VertexUploadConfig& config,
    const VertexUploadApi& api,
    VertexUploadContext& ctx) {
    ctx.device = config.device;
    ctx.queue = config.queue;
    ctx.queue_family_index = config.queue_family_index;
    ctx.staging_buffer = config.staging_buffer;
    ctx.vertex_buffer = config.vertex_buffer;
    ctx.copy_size = config.copy_size;

    ctx.stage = UploadStage::CreatePool;
    CommandPoolAttempt pool_attempt =
        create_graphics_command_pool(
            config.device,
            config.queue_family_index,
            api.create_pool);

    ctx.last_result = pool_attempt.result;
    ctx.last_result_valid = true;
    if (pool_attempt.result != VK_SUCCESS) {
        return snapshot(ctx);
    }
    ctx.upload_pool = pool_attempt.pool;

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = ctx.upload_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer temp_cmd = VK_NULL_HANDLE;

    ctx.stage = UploadStage::Allocate;
    ctx.last_result = api.allocate(config.device, &alloc_info, &temp_cmd);
    ctx.last_result_valid = true;
    if(ctx.last_result != VK_SUCCESS ){
        return snapshot(ctx);
    }
    ctx.cmd = temp_cmd;

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    ctx.stage = UploadStage::Begin;
    ctx.last_result = api.begin(ctx.cmd, &begin_info);
    ctx.last_result_valid = true;
    if (ctx.last_result != VK_SUCCESS) {
        return snapshot(ctx);
    }

    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = ctx.copy_size;
    api.cmd_copy(ctx.cmd, ctx.staging_buffer, ctx.vertex_buffer, 1, &region);

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = config.consumer_access;

    api.cmd_barrier(
        ctx.cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr);


    ctx.stage = UploadStage::EndCommandBuffer;
    ctx.last_result = api.end(ctx.cmd);
    ctx.last_result_valid = true;
    if (ctx.last_result != VK_SUCCESS) {
        return snapshot(ctx);
    }

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = 0;

    VkFence temp_fence = VK_NULL_HANDLE;
    ctx.stage = UploadStage::CreateFence;
    ctx.last_result = api.create_fence(ctx.device,&fence_info,nullptr,&temp_fence);
    ctx.last_result_valid = true;
    if (ctx.last_result != VK_SUCCESS) {
        return snapshot(ctx);
    }
    ctx.upload_fence = temp_fence;

    ctx.state = UploadState::Prepared;
    ctx.stage = UploadStage::CreateFence;
    ctx.last_result = VK_SUCCESS;
    ctx.last_result_valid = true;
    ctx.submit_attempted = false;
    ctx.submit_success = false;
    return snapshot(ctx);
}

UploadAttempt try_complete_vertex_upload(
    VertexUploadContext& ctx,
    const VertexUploadApi& api,
    std::uint64_t timeout_ns) {
    if(ctx.state == UploadState::NotPrepared || ctx.state == UploadState::Blocked ||
       ctx.state == UploadState::Complete)return snapshot(ctx);

    if(ctx.state == UploadState::Prepared){
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &ctx.cmd;

        ctx.stage = UploadStage::Submit;
        ctx.submit_attempted = true;
        VkResult submit_result = api.submit(ctx.queue, 1, &submit_info, ctx.upload_fence);
        ctx.last_result = submit_result;
        ctx.last_result_valid = true;
        if(ctx.last_result != VK_SUCCESS){
            ctx.state = UploadState::Blocked;
            ctx.submit_success = false;
            return snapshot(ctx);
        }
        ctx.state = UploadState::Pending;
        ctx.submit_success = true;
    }

    ctx.stage = UploadStage::Wait;
    VkResult wait_result =
        api.wait(ctx.device, 1, &ctx.upload_fence, VK_TRUE, timeout_ns);
    ctx.last_result = wait_result;
    ctx.last_result_valid = true;

    if (wait_result == VK_SUCCESS) {
        ctx.state = UploadState::Complete;
        return snapshot(ctx);
    }
    if (wait_result == VK_TIMEOUT) {
        // 保持 Pending，资源全部保留
        return snapshot(ctx);
    }
    // 其他失败
    ctx.state = UploadState::Blocked;
    return snapshot(ctx);
}


void destroy_vertex_upload(
    VertexUploadContext& ctx,
    const VertexUploadApi& api) {
    if(ctx.state == UploadState::Pending || ctx.state == UploadState::Blocked){
        return;
    }
    // 1. fence：非空才销毁
    if (ctx.upload_fence != VK_NULL_HANDLE) {
        api.destroy_fence(ctx.device, ctx.upload_fence, nullptr);
        ctx.upload_fence = VK_NULL_HANDLE;
    }

    // 2. pool：非空才销毁；pool 会回收由它分配的 cmd
    if (ctx.upload_pool != VK_NULL_HANDLE) {
        api.destroy_pool(ctx.device, ctx.upload_pool, nullptr);
        ctx.upload_pool = VK_NULL_HANDLE;
    }

    // 3. cmd 是 pool 拥有的句柄，随 pool 清理后置空；不单独 free。
    ctx.cmd = VK_NULL_HANDLE;
}
