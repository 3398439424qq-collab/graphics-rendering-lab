// Initial scaffold prepared by Codex; implementation follows the reviewed image-upload contract.
#include "image_upload.h"
#include "command_recording.h"

namespace {
// Mechanical result mapping supplied by Codex, not student behavior code.
UploadAttempt snapshot(const ImageUploadContext& ctx) {
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

UploadAttempt prepare_image_upload(
    const ImageUploadConfig& config,
    const ImageUploadApi& api,
    ImageUploadContext& ctx) {

    if (ctx.state != UploadState::NotPrepared ||
        ctx.last_result_valid ||
        ctx.upload_pool != VK_NULL_HANDLE ||
        ctx.upload_fence != VK_NULL_HANDLE ||
        ctx.cmd != VK_NULL_HANDLE) {
        return snapshot(ctx);
    }

    // 1. 保存借用句柄
    ctx.device = config.device;
    ctx.queue = config.queue;
    ctx.queue_family_index = config.queue_family_index;
    ctx.staging_buffer = config.staging_buffer;
    ctx.image = config.image;
    ctx.extent = config.extent;

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

    VkBufferImageCopy region{ };
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0,0,0};
    region.imageExtent = {2,2,1};

    VkImageMemoryBarrier before_copy{};
    before_copy.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    before_copy.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    before_copy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    before_copy.srcAccessMask = 0;
    before_copy.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    before_copy.image = ctx.image;
    before_copy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    before_copy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    before_copy.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 1,  // 起始 mip、mip 数量
        0, 1   // 起始数组层、层数量
    };

    VkImageMemoryBarrier after_copy = before_copy;
    after_copy.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    after_copy.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    after_copy.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    after_copy.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = 0;
    
    api.cmd_barrier(
        ctx.cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &before_copy);

    api.cmd_copy(
        ctx.cmd,
    	ctx.staging_buffer,
        ctx.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,&region);
    
    api.cmd_barrier(
        ctx.cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &after_copy);
    
    ctx.stage = UploadStage::EndCommandBuffer;
    ctx.last_result = api.end(ctx.cmd);
    ctx.last_result_valid = true;
    if (ctx.last_result != VK_SUCCESS) {
        return snapshot(ctx);
    }

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

UploadAttempt try_complete_image_upload(
    ImageUploadContext& ctx,
    const ImageUploadApi& api,
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

void destroy_image_upload(
    ImageUploadContext& ctx,
    const ImageUploadApi& api) {
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
