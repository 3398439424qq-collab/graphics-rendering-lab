// 设备选择：从枚举结果中挑出第一块带图形队列族的 NVIDIA 独显，返回设备下标和队列族下标。
#pragma once
#include "device_info.h"
#include "queue_families.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// Given value result: original device position and original queue-family index.
struct DeviceSelection {
    std::size_t device_index;
    std::uint32_t queue_family_index;
};
// Preconditions: equally sized, same-order snapshots from one Complete enumeration.
// Empty inputs are allowed. No Vulkan calls, input mutation/retention or ownership.
// First original device with vendor 0x10de, DISCRETE type and a positive-capacity
// graphics family selected by find_graphics_queue_family. Zero indices are valid.
// Return nullopt when absent; no fallback, logging, sorting or device creation.
std::optional<DeviceSelection> select_target_graphics_device(
    const std::vector<DeviceInfo>& infos,
    const std::vector<std::vector<QueueFamilyInfo>>& families);
