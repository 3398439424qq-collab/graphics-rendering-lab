#include "surface_support.h"

// Initial scaffold prepared by Codex.
SurfaceSupportAttempt query_surface_support(
    VkPhysicalDevice physical_device,
    std::uint32_t queue_family_index,
    VkSurfaceKHR surface,
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR query_fn) {
    VkBool32 sup;
    SurfaceSupportAttempt out;
    auto vkresult = query_fn(physical_device,queue_family_index,surface,&sup);
    if(vkresult != VK_SUCCESS){
        out.result = vkresult;
        out.supported = VK_FALSE;
        return out;
    }
    out.result = vkresult;
    out.supported = sup;
    return out;
}
