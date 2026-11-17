// 枚举模块：按“先取数量、再填数组”两步查询物理设备、设备扩展和实例扩展，
// 用 EnumerationStatus 区分 完整 / 为空 / 取数失败 / 填充失败 / 不完整。
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
enum class EnumerationStatus { Complete, Empty, CountFailed, FillFailed, Incomplete };
struct DeviceEnumeration {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    EnumerationStatus status = EnumerationStatus::CountFailed;
    std::uint32_t count = 0;
    std::vector<VkPhysicalDevice> devices;
};

struct DeviceExtensionEnumeration {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    EnumerationStatus status = EnumerationStatus::CountFailed;
    std::uint32_t count = 0;
    std::vector<VkExtensionProperties> extensions;
};

// Given interface. Borrowed valid instance and non-null function; no ownership transfer.
// One count/fill transaction, no retries. Allocation exceptions are outside this API-result contract.
DeviceEnumeration enumerate_devices(VkInstance instance, PFN_vkEnumeratePhysicalDevices enumerate_fn);
DeviceExtensionEnumeration enumerate_device_extensions(
    VkPhysicalDevice device, PFN_vkEnumerateDeviceExtensionProperties enumerate_fn);

// 实例扩展枚举结果；默认值表示尚未成功枚举。
struct InstanceExtensionEnumeration {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    EnumerationStatus status = EnumerationStatus::CountFailed;
    std::uint32_t count = 0;
    std::vector<VkExtensionProperties> extensions{};
};

// 查询实例扩展（pLayerName = nullptr）；enumerate_fn 须非空，不创建、持有或销毁 Vulkan 对象。
InstanceExtensionEnumeration enumerate_instance_extensions(
    PFN_vkEnumerateInstanceExtensionProperties enumerate_fn);