// 呈现支持检查：查询指定队列族能否把画面呈现到该表面。
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

// Codex-provided reviewed value carrier; owns no input handles.
struct SurfaceSupportAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkBool32 supported = VK_FALSE;
};
// Valid borrowed physical/surface from one live instance; valid original family.
// query_fn is non-null and synchronous. Exactly one call, first three API
// arguments unchanged, fourth is an address of local output storage.
// Preserve VkResult; success copies supported, failure clears it to VK_FALSE.
// No retained handles/pointers, destruction, extra queries, resources or logging.
SurfaceSupportAttempt query_surface_support(
    VkPhysicalDevice physical_device,
    std::uint32_t queue_family_index,
    VkSurfaceKHR surface,
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR query_fn);
