// Declarations assembled by Codex from the reviewed VK04 sections 15/17 contract.
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

enum class UploadState {
    NotPrepared, Prepared, Pending, Complete, Blocked
};

enum class UploadStage {
    None, CreatePool, Allocate, Begin,
    EndCommandBuffer, CreateFence, Submit, Wait
};

struct UploadAttempt {
    UploadState state = UploadState::NotPrepared;
    UploadStage stage = UploadStage::None;
    bool api_result_valid = false;
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    bool submit_attempted = false;
    bool submit_success = false;
};

struct VertexUploadConfig {
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queue_family_index = UINT32_MAX;
    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceSize copy_size = 0;
    VkAccessFlags consumer_access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
};

struct VertexUploadApi {
    PFN_vkCreateCommandPool create_pool = nullptr;
    PFN_vkCreateFence create_fence = nullptr;
    PFN_vkAllocateCommandBuffers allocate = nullptr;
    PFN_vkBeginCommandBuffer begin = nullptr;
    PFN_vkCmdCopyBuffer cmd_copy = nullptr;
    PFN_vkCmdPipelineBarrier cmd_barrier = nullptr;
    PFN_vkEndCommandBuffer end = nullptr;
    PFN_vkQueueSubmit submit = nullptr;
    PFN_vkWaitForFences wait = nullptr;
    PFN_vkDestroyFence destroy_fence = nullptr;
    PFN_vkDestroyCommandPool destroy_pool = nullptr;
};

struct VertexUploadContext {
    VkCommandPool upload_pool = VK_NULL_HANDLE;
    VkFence upload_fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queue_family_index = UINT32_MAX;
    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceSize copy_size = 0;
    UploadState state = UploadState::NotPrepared;
    UploadStage stage = UploadStage::None;
    VkResult last_result = VK_ERROR_INITIALIZATION_FAILED;
    bool last_result_valid = false;
    bool submit_attempted = false;
    bool submit_success = false;

    VertexUploadContext() = default;
    ~VertexUploadContext() = default;
    VertexUploadContext(const VertexUploadContext&) = delete;
    VertexUploadContext& operator=(const VertexUploadContext&) = delete;
    VertexUploadContext(VertexUploadContext&&) = delete;
    VertexUploadContext& operator=(VertexUploadContext&&) = delete;
};

UploadAttempt prepare_vertex_upload(
    const VertexUploadConfig& config,
    const VertexUploadApi& api,
    VertexUploadContext& context);

UploadAttempt try_complete_vertex_upload(
    VertexUploadContext& context,
    const VertexUploadApi& api,
    std::uint64_t timeout_ns);

void destroy_vertex_upload(
    VertexUploadContext& context,
    const VertexUploadApi& api);

// prepare: valid inputs/callbacks, fresh empty NotPrepared context, once only.
// Own pool/fence; pool owns cmd. Other handles borrowed. No implicit destruction.
// Failure retains successful prefix. CreateFence follows End; only all success is Prepared.
// try_complete: NotPrepared/Complete/Blocked revisits make no API calls.
// Pending waits only; Timeout keeps Pending. No reset or repeat submit.
// destroy: Pending/Blocked is a no-op. Safe cleanup nulls owned handles; keeps facts.
// No reuse/reprepare after destruction. See task section 18 for caller contract.
