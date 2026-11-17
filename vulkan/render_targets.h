// 渲染目标模块：创建单颜色附件的 render pass（清屏 → 绘制 → 转为可呈现布局），以及绑定图像视图的帧缓冲。
#pragma once
#include <vulkan/vulkan.h>

struct ColorRenderPassConfig {
    VkDevice device = VK_NULL_HANDLE;
    VkFormat color_format = VK_FORMAT_UNDEFINED;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
};

struct RenderPassAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkRenderPass render_pass = VK_NULL_HANDLE;
};

struct FramebufferConfig {
    VkDevice device = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkImageView color_view = VK_NULL_HANDLE;
    VkExtent2D extent = {0, 0};
    VkImageView depth_view = VK_NULL_HANDLE;
};

struct FramebufferAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

RenderPassAttempt create_color_render_pass(const ColorRenderPassConfig& config,
                                           PFN_vkCreateRenderPass create_fn);

FramebufferAttempt create_color_framebuffer(const FramebufferConfig& config,
                                            PFN_vkCreateFramebuffer create_fn);