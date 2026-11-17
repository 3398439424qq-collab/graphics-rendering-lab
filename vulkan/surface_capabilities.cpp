#include "surface_capabilities.h"

// Initial scaffold prepared by Codex.
SurfaceCapabilitiesAttempt query_surface_capabilities(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR query_fn) {
    SurfaceCapabilitiesAttempt out;
    VkSurfaceCapabilitiesKHR local{};

    out.result = query_fn(physical_device, surface, &local);
    if(out.result != VK_SUCCESS)return out;
    out.capabilities = local;
    return out;
}
