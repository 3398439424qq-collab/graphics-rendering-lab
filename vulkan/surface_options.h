// 表面选项查询：用两步枚举列出表面支持的像素格式与呈现模式，供交换链参数选择。
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

enum class SurfaceListStatus { Complete, Empty, CountFailed, FillFailed, Incomplete };
struct SurfaceFormatsSnapshot {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    SurfaceListStatus status = SurfaceListStatus::CountFailed;
    std::uint32_t count = 0;
    std::vector<VkSurfaceFormatKHR> formats;
};
struct PresentModesSnapshot {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    SurfaceListStatus status = SurfaceListStatus::CountFailed;
    std::uint32_t count = 0;
    std::vector<VkPresentModeKHR> modes;
};
// Given interface: borrowed live, same-instance device/surface; supported surface; non-null query_fn.
// Own returned value snapshots only. At most one count/fill pair per function, no retries.
// Preserve raw last result, order/pairs and only written entries. Failure outputs are not consumed.
// Allocation exceptions are outside this API-result contract. No logging, resource destruction or storage.
SurfaceFormatsSnapshot query_surface_formats(
    VkPhysicalDevice device, VkSurfaceKHR surface,
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR query_fn);
PresentModesSnapshot query_surface_present_modes(
    VkPhysicalDevice device, VkSurfaceKHR surface,
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR query_fn);
