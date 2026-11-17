// 表面能力查询：获取表面支持的图像数量范围、尺寸范围、变换与合成方式，供交换链规划使用。
#pragma once
#include <vulkan/vulkan.h>

// Codex-provided reviewed value carrier. Owns no input handles.
struct SurfaceCapabilitiesAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkSurfaceCapabilitiesKHR capabilities{};
};
// Borrow valid physical/surface from one live instance; support is established.
// query_fn is non-null and synchronous. Exactly one API call, original handles,
// local output address; preserve VkResult. Copy full capabilities only on success.
// On failure do not read API output; retain zero placeholder in returned carrier.
// No retained pointer/handle ownership, resources, logging or extra API calls.
SurfaceCapabilitiesAttempt query_surface_capabilities(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR query_fn);
