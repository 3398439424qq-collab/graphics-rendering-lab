// Declarations assembled by Codex from the reviewed VK05 upload contract.
#pragma once
#include "buffer_upload.h"
#include <cstdint>

struct ImageUploadConfig {
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queue_family_index = UINT32_MAX;
    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkExtent3D extent{};
};

struct ImageUploadApi {
    PFN_vkCreateCommandPool create_pool = nullptr;
    PFN_vkCreateFence create_fence = nullptr;
    PFN_vkAllocateCommandBuffers allocate = nullptr;
    PFN_vkBeginCommandBuffer begin = nullptr;
    PFN_vkCmdCopyBufferToImage cmd_copy = nullptr;
    PFN_vkCmdPipelineBarrier cmd_barrier = nullptr;
    PFN_vkEndCommandBuffer end = nullptr;
    PFN_vkQueueSubmit submit = nullptr;
    PFN_vkWaitForFences wait = nullptr;
    PFN_vkDestroyFence destroy_fence = nullptr;
    PFN_vkDestroyCommandPool destroy_pool = nullptr;
};

struct ImageUploadContext {
    VkCommandPool upload_pool = VK_NULL_HANDLE;
    VkFence upload_fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queue_family_index = UINT32_MAX;
    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkExtent3D extent{};
    UploadState state = UploadState::NotPrepared;
    UploadStage stage = UploadStage::None;
    VkResult last_result = VK_ERROR_INITIALIZATION_FAILED;
    bool last_result_valid = false;
    bool submit_attempted = false;
    bool submit_success = false;

    ImageUploadContext() = default;
    ~ImageUploadContext() = default;
    ImageUploadContext(const ImageUploadContext&) = delete;
    ImageUploadContext& operator=(const ImageUploadContext&) = delete;
    ImageUploadContext(ImageUploadContext&&) = delete;
    ImageUploadContext& operator=(ImageUploadContext&&) = delete;
};

UploadAttempt prepare_image_upload(
    const ImageUploadConfig& config,
    const ImageUploadApi& api,
    ImageUploadContext& ctx);

UploadAttempt try_complete_image_upload(
    ImageUploadContext& ctx,
    const ImageUploadApi& api,
    std::uint64_t timeout_ns);

void destroy_image_upload(
    ImageUploadContext& ctx,
    const ImageUploadApi& api);

// Preconditions: valid callbacks/handles, a fresh empty NotPrepared context,
// and this package's 2x2x1 RGBA8 image (one mip, array layer and sample).
// The host-written staging buffer contains 16 valid bytes and is not submitted;
// the image starts UNDEFINED. Upload and consumption use the same graphics queue.
// prepare is once only: record pre-barrier, copy and post-barrier; do not submit.
// CreateFence follows End; only all-success becomes Prepared. Failure retains
// the successfully acquired prefix for the caller's safe cleanup decision.
// Context owns pool/fence; pool owns cmd. All other handles are borrowed.
// The default destructor releases nothing. No implicit destruction or reuse.
// try_complete submits Prepared once. Pending only waits; Timeout stays Pending.
// NotPrepared/Complete/Blocked revisits make no API calls. Blocked keeps resources.
// destroy is a no-op for Pending/Blocked. Safe states release only owned
// fence/pool, clear cmd, and keep facts; borrowed resources are never destroyed.
