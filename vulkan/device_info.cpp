#include "device_info.h"
// Initial scaffold prepared by Codex.
DeviceInfo query_device_info(VkPhysicalDevice device, PFN_vkGetPhysicalDeviceProperties query_fn) {
    VkPhysicalDeviceProperties props{};
    query_fn(device, &props);
    DeviceInfo out;
    out.deviceName = std::string(props.deviceName);
    out.deviceType = props.deviceType;
    out.apiVersion = props.apiVersion;
    out.vendorID = props.vendorID;
    out.deviceID = props.deviceID;
    return out;
}