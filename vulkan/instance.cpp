#include "instance.h"

// Initial scaffold prepared by Codex.
InstanceAttempt create_instance(const InstanceConfig& config, PFN_vkCreateInstance create_fn) {
    InstanceAttempt out;
    out.result = VK_SUCCESS;
    out.instance = VK_NULL_HANDLE;

    VkApplicationInfo info{};
    info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    info.pNext = nullptr;
    info.pApplicationName = config.application_name;
    info.apiVersion = config.api_version;
    info.applicationVersion = VK_MAKE_API_VERSION(0,0,1,0);
    info.pEngineName = nullptr;
    info.engineVersion = 0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.pApplicationInfo = &info;
    create_info.enabledLayerCount = static_cast<uint32_t>(config.layers.size());
    create_info.ppEnabledLayerNames = config.layers.empty() ? nullptr : config.layers.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(config.extensions.size());
    create_info.ppEnabledExtensionNames = config.extensions.empty() ? nullptr : config.extensions.data();

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = create_fn(&create_info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        out.result = result;              // 保留精确 VkResult
        out.instance = VK_NULL_HANDLE;    // 失败丢弃入口可能写入的非空值
        return out;
    }

    out.result = VK_SUCCESS;
    out.instance = instance;              // 成功才把实例交给调用者
    return out;

}
