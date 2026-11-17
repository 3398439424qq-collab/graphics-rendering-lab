#include "buffer_resources.h"
#include <cstring>

// Initial scaffold prepared by Codex; implementation follows the reviewed buffer-allocation contract.
std::optional<std::uint32_t> select_memory_type(
    const VkPhysicalDeviceMemoryProperties& props,
    std::uint32_t memory_type_bits,
    VkMemoryPropertyFlags required_flags) {
    // Select the smallest allowed memory type with all required flags.
    for(size_t i = 0; i < props.memoryTypeCount; i++){
        const bool type_supported = (memory_type_bits & (1u << i)) != 0;
        const bool flags_satisfied = (props.memoryTypes[i].propertyFlags & required_flags) == required_flags;
        if (type_supported && flags_satisfied) {
            return static_cast<std::uint32_t>(i);
        }
    }
    return std::nullopt;
}

BufferAllocationAttempt create_buffer_allocation(
    VkDevice device,
    const BufferAllocationConfig& config,
    BufferAllocation& allocation,
    const BufferResourcesApi& api) {
    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = config.size;
    buf_info.usage = config.usage;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer raw_buffer = VK_NULL_HANDLE;
    VkResult res = api.create_buffer(device, &buf_info, nullptr, &raw_buffer);
    if (res != VK_SUCCESS) {
        return BufferAllocationAttempt{BufferAllocationStage::CreateBuffer, false, true, res};
    }
    // 仅成功后，才正式认领并记录到 owner 中！
    allocation.buffer = raw_buffer;
    allocation.size = config.size;

    VkMemoryRequirements mem_reqs{};
    api.get_buffer_memory_requirements(device, allocation.buffer, &mem_reqs);
    auto type_opt = select_memory_type(config.mem_props, mem_reqs.memoryTypeBits, config.required_flags);
    if(!type_opt.has_value())return BufferAllocationAttempt{BufferAllocationStage::SelectMemoryType, false, false, VK_ERROR_INITIALIZATION_FAILED};
    const uint32_t chosen_type = *type_opt;
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size; // ⚠️ 注意使用驱动要求的分配量
    alloc_info.memoryTypeIndex = chosen_type;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    res = api.allocate_memory(device, &alloc_info, nullptr, &memory);
    if(res != VK_SUCCESS)return BufferAllocationAttempt{BufferAllocationStage::AllocateMemory, false, true, res};
    allocation.memory = memory;
    allocation.allocation_size = mem_reqs.size;
    allocation.memory_type_index = chosen_type;
    allocation.property_flags = config.mem_props.memoryTypes[chosen_type].propertyFlags;

    res = api.bind_buffer_memory(device, allocation.buffer, allocation.memory, 0);
    if(res != VK_SUCCESS)return BufferAllocationAttempt{BufferAllocationStage::BindBuffer, false, true, res};

    return BufferAllocationAttempt{BufferAllocationStage::Complete, true, true, VK_SUCCESS};
}

VkResult write_host_coherent_buffer(
    VkDevice device,
    const BufferAllocation& allocation,
    const void* cpu_data,
    VkDeviceSize cpu_data_size,
    const BufferResourcesApi& api) {
    // Map, copy only on success, then unmap.
    void* mapped_ptr = nullptr;
    VkResult res = api.map_memory(device, allocation.memory, 0, cpu_data_size, 0, &mapped_ptr);
    if(res != VK_SUCCESS) return res;
    std::memcpy(mapped_ptr, cpu_data, static_cast<size_t>(cpu_data_size));
    api.unmap_memory(device, allocation.memory);
    return VK_SUCCESS;
}

void destroy_buffer_allocation(
    VkDevice device,
    BufferAllocation& allocation,
    const BufferResourcesApi& api) {
    // Destroy owned handles and reset allocation metadata.
    if (allocation.buffer != VK_NULL_HANDLE) {
        api.destroy_buffer(device, allocation.buffer, nullptr);
        allocation.buffer = VK_NULL_HANDLE;
    }
    if (allocation.memory != VK_NULL_HANDLE) {
        api.free_memory(device, allocation.memory, nullptr);
        allocation.memory = VK_NULL_HANDLE;
    }
    allocation.size = 0;
    allocation.allocation_size = 0;
    allocation.property_flags = 0;
    allocation.memory_type_index = UINT32_MAX;
}
