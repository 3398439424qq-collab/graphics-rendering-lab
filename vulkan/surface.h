// 表面模块：通过 GLFW 为窗口创建 VkSurfaceKHR，作为 Vulkan 与窗口系统之间的连接点。
#pragma once
#include <vulkan/vulkan.h>
struct GLFWwindow;
// Given approved interface. Valid instance/window, required extensions enabled,
// non-null output pointer. One call, nullptr allocator, exact result, failure null.
// Success transfers the surface to main. Does not clean up borrowed objects.
VkResult create_window_surface(VkInstance instance, GLFWwindow* window,
                               VkSurfaceKHR* out_surface) noexcept;
