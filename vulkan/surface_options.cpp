#include "surface_options.h"

// Initial scaffolds prepared by Codex.
SurfaceFormatsSnapshot query_surface_formats(
    VkPhysicalDevice device, VkSurfaceKHR surface,
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR query_fn) {
    SurfaceFormatsSnapshot out;

    out.result = query_fn(device,surface,&out.count,nullptr);
    if(out.result != VK_SUCCESS){
        out.status = SurfaceListStatus::CountFailed;
        out.count = 0;
        return out;
    }
    if(out.count == 0){
        out.status = SurfaceListStatus::Empty;
        return out;
    }
    out.formats.resize(out.count);
    out.result = query_fn(device,surface,&out.count,out.formats.data());
    
    if(out.result == VK_SUCCESS){
        if(out.count > 0){
            out.status = SurfaceListStatus::Complete;
        }else{
            out.status = SurfaceListStatus::Empty;
        }
    }else if(out.result == VK_INCOMPLETE){
        out.status = SurfaceListStatus::Incomplete;
    }else{
        out.status = SurfaceListStatus::FillFailed;
        out.count = 0;
        out.formats.clear();
        return out;
    }
    out.formats.resize(out.count);
    return out;
}
PresentModesSnapshot query_surface_present_modes(
    VkPhysicalDevice device, VkSurfaceKHR surface,
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR query_fn) {
    PresentModesSnapshot out;

    out.result = query_fn(device,surface,&out.count,nullptr);
    if(out.result != VK_SUCCESS){
        out.status = SurfaceListStatus::CountFailed;
        out.count = 0;
        return out;
    }
    if(out.count == 0){
        out.status = SurfaceListStatus::Empty;
        return out;
    }
    out.modes.resize(out.count);
    out.result = query_fn(device,surface,&out.count,out.modes.data());
    
    if(out.result == VK_SUCCESS){
        if(out.count > 0){
            out.status = SurfaceListStatus::Complete;

        }else{
            out.status = SurfaceListStatus::Empty;
        }
    }else if(out.result == VK_INCOMPLETE){
        out.status = SurfaceListStatus::Incomplete;
    }else{
        out.status = SurfaceListStatus::FillFailed;
        out.count = 0;
        out.modes.clear();
        return out;
    }
    out.modes.resize(out.count);
    return out;
}
