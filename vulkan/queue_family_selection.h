// 队列族选择：在一台设备的队列族快照中，找出第一个支持图形且队列数大于 0 的族。
#pragma once
#include "queue_families.h"
#include <optional>

// Given policy: one device's owned snapshots, read-only; empty input is allowed.
// First entry in input order with positive capacity and GRAPHICS bit.
// Return the entry's original familyIndex, including 0; nullopt if no match.
// No Vulkan calls, logging, device selection, queue creation, or input retention.
std::optional<std::uint32_t> find_graphics_queue_family(
    const std::vector<QueueFamilyInfo>& families);
