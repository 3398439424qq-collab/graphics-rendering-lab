// 实例模块：按配置（应用名、API 版本、扩展、层）创建 VkInstance，成功才把句柄交给调用者。
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

// Given interface, Codex. Valid strings/arrays and non-null create_fn are preconditions.
struct InstanceConfig {
    const char* application_name;
    std::uint32_t api_version;
    std::vector<const char*> extensions;
    std::vector<const char*> layers;
};
struct InstanceAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkInstance instance = VK_NULL_HANDLE;
};
// Success transfers the instance to the caller; caller later destroys it.
// Failure preserves the exact result and returns a null instance.
// This function performs no logging, enumeration, window creation or destruction.
InstanceAttempt create_instance(const InstanceConfig& config, PFN_vkCreateInstance create_fn);
