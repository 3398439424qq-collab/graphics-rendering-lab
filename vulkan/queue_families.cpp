#include "queue_families.h"

// Initial scaffold prepared by Codex; implementation follows the reviewed queue-family contract.
std::vector<QueueFamilyInfo> query_queue_families(
    VkPhysicalDevice device, PFN_vkGetPhysicalDeviceQueueFamilyProperties query_fn) {
    std::vector<QueueFamilyInfo> out;
    
    if (device == VK_NULL_HANDLE || query_fn == nullptr) {
        return out;
    }

    std::uint32_t count = 0;
    query_fn(device, &count, nullptr);

    if(count == 0)return out;

    std::vector<VkQueueFamilyProperties> props(count);
    std::uint32_t written = count;  // 调用前表示容量，调用后表示实际写入数
    query_fn(device, &written, props.data());

    out.reserve(written);

    for(std::uint32_t i = 0; i < written; ++i){
        QueueFamilyInfo info;
        info.familyIndex = i;                    
        info.queueFlags = props[i].queueFlags;
        info.queueCount = props[i].queueCount;
        out.push_back(info);
    }

    return out;
}
