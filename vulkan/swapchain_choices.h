// 交换链参数选择：纯函数，根据表面能力和可选项决定图像数量、尺寸、格式、呈现模式与合成方式。
#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

// Precondition: caps is a successfully queried valid capability snapshot.
// Borrow inputs only; no Vulkan/GLFW calls, stored pointers, or resources.
// 图像数量：优先 3（三缓冲），并限制在表面允许的 [min, max] 内。
std::uint32_t choose_swapchain_image_count(const VkSurfaceCapabilitiesKHR& caps);
// 尺寸：currentExtent 已确定就直接用，否则把帧缓冲尺寸夹到允许范围；零尺寸（如最小化）返回 nullopt。
std::optional<VkExtent2D> choose_swapchain_extent(
    const VkSurfaceCapabilitiesKHR& caps, VkExtent2D framebuffer_extent);
// 格式：优先 B8G8R8A8_SRGB + SRGB_NONLINEAR，否则退回列表第一项。
std::optional<VkSurfaceFormatKHR> choose_swapchain_surface_format(
    const std::vector<VkSurfaceFormatKHR>& formats);
bool supports_linear_srgb_output(
    const VkSurfaceFormatKHR& surface_format);
// 呈现模式：优先 MAILBOX，其次 FIFO，都没有则返回 nullopt。
std::optional<VkPresentModeKHR> choose_swapchain_present_mode(
    const std::vector<VkPresentModeKHR>& modes);
// 合成 alpha：按 OPAQUE → PRE_MULTIPLIED → POST_MULTIPLIED → INHERIT 顺序取第一个受支持的。
std::optional<VkCompositeAlphaFlagBitsKHR> choose_swapchain_composite_alpha(
    VkCompositeAlphaFlagsKHR supported);

