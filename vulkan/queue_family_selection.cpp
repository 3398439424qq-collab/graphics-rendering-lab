#include "queue_family_selection.h"

// Initial scaffold prepared by Codex.
std::optional<std::uint32_t> find_graphics_queue_family(
    const std::vector<QueueFamilyInfo>& families) {
    for (const auto& f : families) {
        if (f.queueCount > 0 &&
            (f.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            return f.familyIndex;
        }
    }
    return std::nullopt;
}
