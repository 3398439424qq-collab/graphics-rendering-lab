// 设备信息模块：读取物理设备属性，转成可打印、可比较的值快照（名称、类型、厂商、API 版本）。
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
struct DeviceInfo {
    std::uint32_t apiVersion=0;
    std::uint32_t vendorID=0;
    std::uint32_t deviceID=0;
    VkPhysicalDeviceType deviceType=VK_PHYSICAL_DEVICE_TYPE_OTHER;
    std::string deviceName;
};
// Given interface: borrowed valid device, live parent instance, non-null function.
// One query, owned value snapshot, no logging or destruction; allocation exceptions out of scope.
DeviceInfo query_device_info(VkPhysicalDevice device, PFN_vkGetPhysicalDeviceProperties query_fn);
