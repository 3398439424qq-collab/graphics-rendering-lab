#include "device_selection.h"
#include "queue_family_selection.h"

// Initial scaffold prepared by Codex.
std::optional<DeviceSelection> select_target_graphics_device(
    const std::vector<DeviceInfo>& infos,
    const std::vector<std::vector<QueueFamilyInfo>>& families) {
    for (std::size_t i = 0; i < infos.size(); ++i) {
        if (infos[i].vendorID != 0x10de ||
                infos[i].deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                continue;   // 不是 NVIDIA 独显，跳过
            }
            const auto family = find_graphics_queue_family(families[i]);
            if (family) {
                return DeviceSelection{i, *family};
            }
        }
    return std::nullopt;
}
