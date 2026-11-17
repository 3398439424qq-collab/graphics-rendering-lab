#include "swapchain.h"

SwapchainAttempt create_swapchain(
    const SwapchainConfig& config, PFN_vkCreateSwapchainKHR create_fn){
    VkSwapchainCreateInfoKHR attempt{};
    attempt.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    attempt.pNext = nullptr;
    attempt.flags = 0;
    attempt.imageArrayLayers = 1;
    attempt.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    attempt.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    attempt.queueFamilyIndexCount = 0;
    attempt.pQueueFamilyIndices = nullptr;
    attempt.clipped = VK_TRUE;

    attempt.surface          = config.surface;
    attempt.minImageCount    = config.min_image_count;
    attempt.imageFormat      = config.surface_format.format;
    attempt.imageColorSpace  = config.surface_format.colorSpace;
    attempt.imageExtent      = config.extent;
    attempt.preTransform     = config.pre_transform;
    attempt.compositeAlpha   = config.composite_alpha;
    attempt.presentMode      = config.present_mode;
    attempt.oldSwapchain = config.old_swapchain;

    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkResult r = create_fn(config.device, &attempt, nullptr, &handle);

    SwapchainAttempt out{};
    out.result = r;
    out.swapchain = (r == VK_SUCCESS) ? handle : VK_NULL_HANDLE;
    out.old_retired = (config.old_swapchain != VK_NULL_HANDLE); 
    return out;
}

SwapchainImagesSnapshot query_swapchain_images(
    VkDevice device, VkSwapchainKHR swapchain, PFN_vkGetSwapchainImagesKHR query_fn){
    SwapchainImagesSnapshot out;
    out.result = query_fn(device,swapchain,&out.count,nullptr);

    if(out.result != VK_SUCCESS){
        out.status = EnumerationStatus::CountFailed;
        out.count = 0;
        out.images.clear();
        return out;
    }

    if(out.count == 0){
        out.status = EnumerationStatus::Empty;
        out.count = 0;
        out.images.clear();
        return out;
    }

    out.images.resize(out.count);
    out.result = query_fn(device,swapchain,&out.count,out.images.data());

    if(out.result == VK_SUCCESS){
        if (out.count == 0) {
            out.status = EnumerationStatus::Empty;
            out.count = 0;
            out.images.clear();
            return out;
        }
        out.status = EnumerationStatus::Complete;
    }else if(out.result == VK_INCOMPLETE){
        out.status = EnumerationStatus::Incomplete;
    }else{
        out.status = EnumerationStatus::FillFailed;
        out.count = 0;
        out.images.clear();
        return out;
    }

    out.images.resize(out.count);
    return out;

    }

