// Declarations assembled by Codex from the reviewed VK04 sections 2/5/6 contract.
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <optional>

// Explicit single owner, no automatic destruction. Caller destroys before device.
struct BufferAllocation {
    BufferAllocation() = default;
    ~BufferAllocation() = default;
    BufferAllocation(const BufferAllocation&) = delete;
    BufferAllocation& operator=(const BufferAllocation&) = delete;
    BufferAllocation(BufferAllocation&&) = delete;
    BufferAllocation& operator=(BufferAllocation&&) = delete;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkDeviceSize allocation_size = 0;
    VkMemoryPropertyFlags property_flags = 0;
    std::uint32_t memory_type_index = UINT32_MAX;
};
struct BufferAllocationConfig {
    VkDeviceSize size = 0;
    VkPhysicalDeviceMemoryProperties mem_props{};
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags required_flags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
};
enum class BufferAllocationStage {
    CreateBuffer, SelectMemoryType, AllocateMemory, BindBuffer, Complete
};
// Reports facts only; never holds a resource owner.
struct BufferAllocationAttempt {
    BufferAllocationStage stage = BufferAllocationStage::CreateBuffer;
    bool complete = false;
    bool api_result_valid = false;
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
};
struct BufferResourcesApi {
    PFN_vkCreateBuffer create_buffer = nullptr;
    PFN_vkGetBufferMemoryRequirements get_buffer_memory_requirements = nullptr;
    PFN_vkAllocateMemory allocate_memory = nullptr;
    PFN_vkBindBufferMemory bind_buffer_memory = nullptr;
    PFN_vkMapMemory map_memory = nullptr;
    PFN_vkUnmapMemory unmap_memory = nullptr;
    PFN_vkDestroyBuffer destroy_buffer = nullptr;
    PFN_vkFreeMemory free_memory = nullptr;
};

// Valid snapshot; return smallest original compatible type index, or nullopt.
std::optional<std::uint32_t> select_memory_type(
    const VkPhysicalDeviceMemoryProperties& props,
    std::uint32_t memory_type_bits,
    VkMemoryPropertyFlags required_flags);

// Valid device/config/callbacks, empty allocation. One non-sparse exclusive buffer.
// Record only successful output handles directly in allocation, first failure stops.
// No selection match: SelectMemoryType/incomplete/invalid API result (ignore result).
// Allocate req.size, bind at offset 0. No cleanup, map, waiting or retained config.
BufferAllocationAttempt create_buffer_allocation(
    VkDevice device,
    const BufferAllocationConfig& config,
    BufferAllocation& allocation,
    const BufferResourcesApi& api);

// Bound/unmapped HOST_VISIBLE|HOST_COHERENT resource, never submitted to GPU.
// Valid CPU range, cpu_data_size == allocation.size and representable by size_t.
// Map offset 0/flags 0; success copies exact logical bytes and unmaps synchronously.
// Return actual map result. On failure no copy/unmap; no retained host pointer.
VkResult write_host_coherent_buffer(
    VkDevice device,
    const BufferAllocation& allocation,
    const void* cpu_data,
    VkDeviceSize cpu_data_size,
    const BufferResourcesApi& api);

// Valid device/callbacks, unmapped resource with no device references in flight.
// Destroy buffer, free memory, reset all fields individually; empty owner is safe.
// No waits or parent destruction; allocator always nullptr.
void destroy_buffer_allocation(
    VkDevice device,
    BufferAllocation& allocation,
    const BufferResourcesApi& api);
