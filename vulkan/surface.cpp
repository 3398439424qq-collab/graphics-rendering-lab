#include "surface.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

VkResult create_window_surface(VkInstance instance, GLFWwindow* window,
                               VkSurfaceKHR* out_surface) noexcept {
    *out_surface = VK_NULL_HANDLE;
    return glfwCreateWindowSurface(instance, window, nullptr, out_surface);
}
