// 逻辑设备模块：创建 VkDevice 并取得图形队列；另含 swapchain_maintenance1 特性的支持查询与按需启用。
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

// Given inputs: live parent instance/physical device, valid original graphics
// family with capacity >= 1, valid supported/required extension names and count.
struct DeviceConfig {
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    std::uint32_t queue_family_index = 0;
    std::vector<const char*> extensions;
    bool enable_swapchain_maintenance1 = false;
};
struct DeviceAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkDevice device = VK_NULL_HANDLE;
};
// Non-null synchronous create_fn; exactly one call, allocator=nullptr.
// Request one queue at priority 1.0f from the configured family; no device layers.
// Default false: no feature chain. True requires supported maintenance1,
// enabled instance dependencies, and both required device extensions in config.
// True: request only swapchainMaintenance1 via a local EXT pNext structure.
// Extension order/names follow config; pEnabledFeatures remains null.
// Success transfers the device to caller; failure keeps exact result/null device.
// No selection, queries, logging, destruction, getQueue, or retained pointers.
DeviceAttempt create_device(const DeviceConfig& config, PFN_vkCreateDevice create_fn);

// Given composition result: a plain carrier, not RAII and not an owner.
// On success the caller owns device; queue is borrowed from that device.
// Caller releases device with nullptr allocator before its parent instance.
struct DeviceQueueAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
};
// Same valid config/create_fn preconditions as create_device; get_queue_fn is
// non-null and synchronous. Reuse create_device exactly once.
// Failure: exact result, both handles null, no get. Success: get once using
// returned device, configured original family, queueIndex=0; return both handles.
// No cleanup, main integration, selection, output, or retained input pointers.
DeviceQueueAttempt create_device_with_queue(
    const DeviceConfig& config,
    PFN_vkCreateDevice create_fn,
    PFN_vkGetDeviceQueue get_queue_fn);


// Codex-frozen VK03 support-query interface (2026-10-31).
// These flags are caller-verified facts, not requests to enumerate or enable.
struct SwapchainMaintenanceQueryConfig {
    VkPhysicalDevice physical_device = VK_NULL_HANDLE; // borrowed
    bool instance_dependencies_enabled = false;
    bool device_extensions_supported = false;
};
enum class SwapchainMaintenanceSupportStatus {
    InstanceDependenciesMissing,
    DeviceExtensionsMissing,
    QueryUnavailable,
    Unsupported,
    Supported
};
struct SwapchainMaintenanceSupportAttempt {
    SwapchainMaintenanceSupportStatus status =
        SwapchainMaintenanceSupportStatus::InstanceDependenciesMissing;
    bool query_called = false;
    bool supported = false;
};
// Check instance facts, then device facts, then callback availability.
// Only the remaining branch requires a valid borrowed physical_device.
// Query exactly once through a synchronous callback using local output chains.
// Void query: no fabricated VkResult, creation, cleanup, waiting or retained pointers.
SwapchainMaintenanceSupportAttempt query_swapchain_maintenance1(
    const SwapchainMaintenanceQueryConfig& config,
    PFN_vkGetPhysicalDeviceFeatures2KHR get_features2);
