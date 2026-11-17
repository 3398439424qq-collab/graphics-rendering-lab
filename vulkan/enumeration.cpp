#include "enumeration.h"
// Initial scaffold prepared by Codex.
DeviceEnumeration enumerate_devices(VkInstance instance, PFN_vkEnumeratePhysicalDevices enumerate_fn) {
    DeviceEnumeration out;
    uint32_t count = 0;
    out.result = enumerate_fn(instance,&count,nullptr);
    if(out.result != VK_SUCCESS){
        out.status = EnumerationStatus::CountFailed;
        out.count = 0;
        out.devices.clear();
        return out;
    }

    if(count == 0){
        out.status = EnumerationStatus::Empty;
        out.count = 0;
        out.devices.clear();
        return out;
    }

    std::uint32_t capacity = count;
    out.devices.resize(count);
    out.result = enumerate_fn(instance,&capacity,out.devices.data());

    if(out.result == VK_SUCCESS){
        if (capacity == 0) {
            out.status = EnumerationStatus::Empty;
            out.count = 0;
            out.devices.clear();
            return out;
        }
        out.status = EnumerationStatus::Complete;
    }else if(out.result == VK_INCOMPLETE){
        out.status = EnumerationStatus::Incomplete;
    }else{
        out.status = EnumerationStatus::FillFailed;
        out.count = 0;
        out.devices.clear();
        return out;
    }

    out.count = capacity;
    out.devices.resize(capacity);
    return out;
}

DeviceExtensionEnumeration enumerate_device_extensions(
    VkPhysicalDevice device, PFN_vkEnumerateDeviceExtensionProperties enumerate_fn){
    DeviceExtensionEnumeration out;
    out.result = enumerate_fn(device,nullptr,&out.count,nullptr);
    if(out.result != VK_SUCCESS){
        out.status = EnumerationStatus::CountFailed;
        out.count = 0;
        out.extensions.clear();
        return out;
    }

    if(out.count == 0){
        out.status = EnumerationStatus::Empty;
        out.count = 0;
        out.extensions.clear();
        return out;
    }

    out.extensions.resize(out.count);
    out.result = enumerate_fn(device,nullptr,&out.count,out.extensions.data());

    if(out.result == VK_SUCCESS){
        if (out.count == 0) {
            out.status = EnumerationStatus::Empty;
            out.count = 0;
            out.extensions.clear();
            return out;
        }
        out.status = EnumerationStatus::Complete;
    }else if(out.result == VK_INCOMPLETE){
        out.status = EnumerationStatus::Incomplete;
    }else{
        out.status = EnumerationStatus::FillFailed;
        out.count = 0;
        out.extensions.clear();
        return out;
    }

    out.extensions.resize(out.count);
    return out;
}

InstanceExtensionEnumeration enumerate_instance_extensions(
    PFN_vkEnumerateInstanceExtensionProperties enumerate_fn){
    InstanceExtensionEnumeration out;
    out.result = enumerate_fn(nullptr, &out.count, nullptr);
    if(out.result != VK_SUCCESS){
        out.status = EnumerationStatus::CountFailed;
        out.count = 0;
        out.extensions.clear();
        return out;
    }
    if(out.count == 0){
        out.status = EnumerationStatus::Empty;
        out.count = 0;
        out.extensions.clear();
        return out;
    }
    out.extensions.resize(out.count);
    out.result = enumerate_fn(nullptr, &out.count, out.extensions.data());
    if(out.result == VK_SUCCESS){
        if (out.count == 0) {
            out.status = EnumerationStatus::Empty;
            out.count = 0;
            out.extensions.clear();
            return out;
        }
        out.status = EnumerationStatus::Complete;
    }else if(out.result == VK_INCOMPLETE){
        out.status = EnumerationStatus::Incomplete;
    }else{
        out.status = EnumerationStatus::FillFailed;
        out.count = 0;
        out.extensions.clear();
        return out;
    }
    out.extensions.resize(out.count);
    return out;
}