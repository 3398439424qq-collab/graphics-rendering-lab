// 图像视图模块：为交换链图像创建 2D 颜色视图，使其能作为帧缓冲的颜色附件。
#pragma once
#include <vulkan/vulkan.h>

// Borrowed handles only; this struct owns nothing.
struct ImageViewConfig {
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
};

// Success: caller owns view. Failure: exact result, null view, output not readable.
struct ImageViewAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkImageView view = VK_NULL_HANDLE;
};

ImageViewAttempt create_image_view(const ImageViewConfig& config,
                                   PFN_vkCreateImageView create_fn);