#pragma once
#include <vulkan/vulkan.h>

// VK05: owner and function declarations submitted by the learner.
// Codex assembled the agreed Config/Stage/Attempt/Api and contract comments.
// Explicit single owner; the default destructor does not release Vulkan resources.
struct ImageAllocation{
    ImageAllocation() = default;
    ~ImageAllocation() = default;
    ImageAllocation(const ImageAllocation&) = delete;
    ImageAllocation& operator=(const ImageAllocation&) = delete;
    ImageAllocation(ImageAllocation&&) = delete;
    ImageAllocation& operator=(ImageAllocation&&) = delete;

    VkImage image         = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct ImageAllocationConfig {
    VkExtent3D extent{};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkPhysicalDeviceMemoryProperties mem_props{};
    VkImageUsageFlags usage = 0;
    VkMemoryPropertyFlags required_flags = 0;
};

enum class ImageAllocationStage {
    CreateImage, SelectMemoryType, AllocateMemory, BindImageMemory, Complete
};

// Reports facts only; does not own resources.
struct ImageAllocationAttempt {
    ImageAllocationStage stage = ImageAllocationStage::CreateImage;
    bool complete = false;
    bool api_result_valid = false;
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
};

struct ImageResourcesApi {
    PFN_vkCreateImage create_image = nullptr;
    PFN_vkGetImageMemoryRequirements get_image_memory_requirements = nullptr;
    PFN_vkAllocateMemory allocate_memory = nullptr;
    PFN_vkBindImageMemory bind_image_memory = nullptr;
    PFN_vkDestroyImage destroy_image = nullptr;
    PFN_vkFreeMemory free_memory = nullptr;
};

// VK06: agreed learner ownership contract, assembled by Codex after review.
// Own allocation only; borrow device and copy the callback table.
// Before reset/destruction: views are gone and no GPU work uses allocation.
// Empty owners make no Vulkan calls. No implicit waits or parent destruction.
struct TextureImageOwner {
    ImageAllocation allocation{};
    VkDevice device = VK_NULL_HANDLE;
    ImageResourcesApi image_api{};

    TextureImageOwner() = default;
    ~TextureImageOwner() noexcept;
    TextureImageOwner(const TextureImageOwner&) = delete;
    TextureImageOwner& operator=(const TextureImageOwner&) = delete;
    TextureImageOwner(TextureImageOwner&& other) noexcept;
    TextureImageOwner& operator=(TextureImageOwner&& other) noexcept;

    // Destroy owned image/memory through the stored context, then restore all
    // members to defaults. Safe for empty or partially initialized allocation.
    void reset() noexcept;

    // destination must be distinct and empty. Transfer handles without Vulkan
    // calls and restore this owner to defaults. Used by terminal keep-alive exits.
    void release_to(ImageAllocation& destination) noexcept;
};

// Valid device/config/callbacks, empty owner. Supported non-sparse 2D image.
// Fixed: one mip/layer/sample, optimal tiling, exclusive sharing, undefined layout.
// Claim successful outputs only; stop at first failure, retaining partial owner.
// Reuse select_memory_type; allocate req.size and bind at offset 0.
// No matching type: SelectMemoryType, incomplete, invalid API result (ignore result).
// Complete: complete=true, api_result_valid=true, result=VK_SUCCESS.
// No internal cleanup, view creation, GPU submission, waiting or retained config.
ImageAllocationAttempt create_image_allocation(VkDevice device,const ImageAllocationConfig& image_config,
    ImageAllocation& image,const ImageResourcesApi& image_api);


// Valid device/callbacks; any view already destroyed; no in-flight references.
// Destroy image, free memory, clear each handle. Empty owner is safe.
// No waiting or parent destruction. Allocation callbacks are always nullptr.
void destroy_image_allocation(VkDevice device,ImageAllocation& image,const ImageResourcesApi& image_api);
