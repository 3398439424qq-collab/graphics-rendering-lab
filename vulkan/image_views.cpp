#include "image_views.h"

ImageViewAttempt create_image_view(const ImageViewConfig& config,
                                   PFN_vkCreateImageView create_fn) {
    VkImageViewCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.image = config.image;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = config.format;
    ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.subresourceRange.aspectMask = config.aspect_mask;
    ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount = 1;

    ImageViewAttempt attempt{};
    attempt.view = VK_NULL_HANDLE;
    attempt.result = create_fn(config.device, &ci, nullptr, &attempt.view);
    if (attempt.result != VK_SUCCESS) {
        attempt.view = VK_NULL_HANDLE; // 保持空，防止垃圾
    }
    return attempt;
}