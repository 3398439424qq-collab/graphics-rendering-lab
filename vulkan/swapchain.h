// 交换链模块：按选定参数创建 VkSwapchainKHR（可传入旧交换链进行替换），并取回其图像列表。
#pragma once
#include "enumeration.h"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

// Inputs for one creation. Borrowed handles only; this struct owns nothing.
struct SwapchainConfig {
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    std::uint32_t min_image_count = 0;
    VkSurfaceFormatKHR surface_format{};
    VkExtent2D extent{0, 0};
    VkSurfaceTransformFlagBitsKHR pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;
};

// Success: caller owns swapchain. Failure: exact result, null handle.
struct SwapchainAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    bool old_retired = false;
};

// Images are borrowed from the swapchain; never destroy them individually.
struct SwapchainImagesSnapshot {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    EnumerationStatus status = EnumerationStatus::CountFailed;
    std::uint32_t count = 0;
    std::vector<VkImage> images;
};

SwapchainAttempt create_swapchain(
    const SwapchainConfig& config, PFN_vkCreateSwapchainKHR create_fn);

SwapchainImagesSnapshot query_swapchain_images(
    VkDevice device, VkSwapchainKHR swapchain, PFN_vkGetSwapchainImagesKHR query_fn);
