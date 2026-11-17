// 队列族模块：查询物理设备的全部队列族，按原顺序输出能力快照（队列标志位与队列数）。
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

struct QueueFamilyInfo {
    std::uint32_t familyIndex = 0;
    VkQueueFlags queueFlags = 0;
    std::uint32_t queueCount = 0;
};

// Given interface: valid borrowed physical device and live instance; non-null query_fn.
// Return owned snapshots of this call's written entries, in original family order.
// Exactly two injected queries (count then fill), no logging/selection/destruction.
// No VkResult or completeness claim. Allocation exceptions are outside this package.
std::vector<QueueFamilyInfo> query_queue_families(
    VkPhysicalDevice device, PFN_vkGetPhysicalDeviceQueueFamilyProperties query_fn);
