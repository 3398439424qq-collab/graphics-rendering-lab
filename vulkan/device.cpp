#include "device.h"

// Initial scaffold prepared by Codex.
DeviceAttempt create_device(const DeviceConfig& config, PFN_vkCreateDevice create_fn) {
    DeviceAttempt out;
    out.result = VK_ERROR_INITIALIZATION_FAILED;
    out.device = VK_NULL_HANDLE;

    float priority = 1.0f;

    VkDeviceQueueCreateInfo q{};
    q.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    q.pNext = nullptr;
    q.flags = 0;
    q.queueFamilyIndex = config.queue_family_index;
    q.queueCount = 1;                                       
    q.pQueuePriorities = &priority;

    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maint1{};
    maint1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    maint1.pNext = nullptr;
    maint1.swapchainMaintenance1 = VK_TRUE;

    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.pNext = config.enable_swapchain_maintenance1 ? &maint1 : nullptr;
    ci.flags = 0;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &q;
    ci.enabledLayerCount = 0;
    ci.ppEnabledLayerNames = nullptr;
    ci.enabledExtensionCount = static_cast<uint32_t>(config.extensions.size());
    ci.ppEnabledExtensionNames = config.extensions.empty()
        ? nullptr
        : config.extensions.data();
    ci.pEnabledFeatures = nullptr;

    VkDevice device = VK_NULL_HANDLE;
    VkResult result = create_fn(config.physical_device, &ci, nullptr, &device);

    out.result = result;
    out.device = (result == VK_SUCCESS) ? device : VK_NULL_HANDLE;
    return out;
}

// Compose device creation with queue retrieval.
DeviceQueueAttempt create_device_with_queue(
    const DeviceConfig& config,
    PFN_vkCreateDevice create_fn,
    PFN_vkGetDeviceQueue get_queue_fn) {
    DeviceQueueAttempt out{};

    DeviceAttempt attempt = create_device(config, create_fn);

    if(attempt.result != VK_SUCCESS) {
        out.result = attempt.result;
        return out;
        }
    out.result = attempt.result;
    out.device = attempt.device;
    VkQueue queue = VK_NULL_HANDLE;
    get_queue_fn(attempt.device,config.queue_family_index,0,&queue);
    out.queue = queue;

    return out;
}

SwapchainMaintenanceSupportAttempt query_swapchain_maintenance1(
    const SwapchainMaintenanceQueryConfig& config,
    PFN_vkGetPhysicalDeviceFeatures2KHR get_features2){
    SwapchainMaintenanceSupportAttempt out{};

    // 第一层：实例依赖门控
    if (!config.instance_dependencies_enabled) {
        out.status = SwapchainMaintenanceSupportStatus::InstanceDependenciesMissing;
        return out;
    }

    // 第二层：设备扩展门控
    if (!config.device_extensions_supported) {
        out.status = SwapchainMaintenanceSupportStatus::DeviceExtensionsMissing;
        return out;
    }

    // 第三层：查询入口门控
    if (get_features2 == nullptr) {
        out.status = SwapchainMaintenanceSupportStatus::QueryUnavailable;
        return out;
    }
    
    // 构造局部输出链
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maint1{};
    maint1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    maint1.pNext = nullptr;
    maint1.swapchainMaintenance1 = VK_FALSE;
    VkPhysicalDeviceFeatures2KHR features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    features2.pNext = &maint1;
    // 实际执行一次查询
    get_features2(config.physical_device, &features2);
    out.query_called = true;
    // 判定驱动写回结果
    if (maint1.swapchainMaintenance1 == VK_TRUE) {
        out.status    = SwapchainMaintenanceSupportStatus::Supported;
        out.supported = true;
    } else {
        out.status    = SwapchainMaintenanceSupportStatus::Unsupported;
        out.supported = false;
    }
    return out;
}