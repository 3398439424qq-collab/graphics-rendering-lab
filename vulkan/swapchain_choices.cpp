#include "swapchain_choices.h"
#include <cstdint>
#include <optional>
#include <vulkan/vulkan.h>
#include <algorithm>

std::uint32_t choose_swapchain_image_count(const VkSurfaceCapabilitiesKHR& caps) {
    if(3 < caps.minImageCount)      return caps.minImageCount;
    else if( caps.maxImageCount != 0 && 3 > caps.maxImageCount )return caps.maxImageCount;
    return 3;
}

std::optional<VkExtent2D> choose_swapchain_extent(
    const VkSurfaceCapabilitiesKHR& caps, VkExtent2D framebuffer_extent) {

    const bool sentinel =
        caps.currentExtent.width == UINT32_MAX &&
        caps.currentExtent.height == UINT32_MAX;
    
    if (framebuffer_extent.width == 0 || framebuffer_extent.height == 0) {
        return std::nullopt;
    }
    
    if (!sentinel) {
        // 确定值路径：先零检查，再直接采用，不碰 framebuffer
        if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
            return std::nullopt;
        }
        return caps.currentExtent;
    }

    VkExtent2D extent{
        std::clamp(framebuffer_extent.width,
                   caps.minImageExtent.width, caps.maxImageExtent.width),
        std::clamp(framebuffer_extent.height,
                   caps.minImageExtent.height, caps.maxImageExtent.height)
    };

    if (extent.width == 0 || extent.height == 0) {
        return std::nullopt;
    }
    return extent;

}

std::optional<VkSurfaceFormatKHR> choose_swapchain_surface_format(
    const std::vector<VkSurfaceFormatKHR>& formats){

    if (formats.empty()) return std::nullopt;

    constexpr VkFormat preferred_format = VK_FORMAT_B8G8R8A8_SRGB;
    constexpr VkColorSpaceKHR preferred_color_space =
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    for (const auto& f : formats) {
        if (f.format == preferred_format && f.colorSpace == preferred_color_space)
            return f;              // 精确配对，整项返回
    }

    return formats.front();        // 回退：第一整项，不拼字段
}


bool supports_linear_srgb_output(
    const VkSurfaceFormatKHR& surface_format){
        if(surface_format.format != VK_FORMAT_B8G8R8A8_SRGB||
        surface_format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
        return false;
        }
        return true;
    }

std::optional<VkPresentModeKHR> choose_swapchain_present_mode(
    const std::vector<VkPresentModeKHR>& modes){
    
    if(modes.empty())return std::nullopt;
    
    for (VkPresentModeKHR m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            return m;
    }

    for(VkPresentModeKHR m : modes){
        if(m == VK_PRESENT_MODE_FIFO_KHR)
            return m;
    }

    return std::nullopt;
}

std::optional<VkCompositeAlphaFlagBitsKHR> choose_swapchain_composite_alpha(
    VkCompositeAlphaFlagsKHR supported){
        if (supported & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    if (supported & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    }
    if (supported & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
        return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }
    if (supported & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
        return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
    return std::nullopt;
}