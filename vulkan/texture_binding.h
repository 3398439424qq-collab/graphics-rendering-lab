#pragma once
#include <vulkan/vulkan.h>

// Declarations assembled by Codex from the reviewed VK05 section 11 contract.
// Explicit owner; the default destructor does not release Vulkan resources.
struct TextureBindingResources {
    TextureBindingResources() = default;
    ~TextureBindingResources() = default;
    TextureBindingResources(const TextureBindingResources&) = delete;
    TextureBindingResources& operator=(const TextureBindingResources&) = delete;
    TextureBindingResources(TextureBindingResources&&) = delete;
    TextureBindingResources& operator=(TextureBindingResources&&) = delete;

    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE; // Reclaimed with pool; never freed separately.
};

enum class TextureBindingStage {
    CreateSampler, CreateSetLayout, CreatePool, AllocateSet, Complete
};

// Reports facts only; ownership remains in TextureBindingResources.
struct TextureBindingAttempt {
    TextureBindingStage stage = TextureBindingStage::CreateSampler;
    bool complete = false;
    bool api_result_valid = false;
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
};

struct TextureBindingApi {
    PFN_vkCreateSampler create_sampler = nullptr;
    PFN_vkCreateDescriptorSetLayout create_set_layout = nullptr;
    PFN_vkCreateDescriptorPool create_pool = nullptr;
    PFN_vkAllocateDescriptorSets allocate_sets = nullptr;
    PFN_vkUpdateDescriptorSets update_sets = nullptr;
    PFN_vkDestroySampler destroy_sampler = nullptr;
    PFN_vkDestroyDescriptorSetLayout destroy_set_layout = nullptr;
    PFN_vkDestroyDescriptorPool destroy_pool = nullptr;
};

// Valid device/view/callbacks, empty owner, and completed image upload.
// Fixed sampler/layout/pool/set-write fields: VK05 task document, section 11.
// Claim each successful VkResult output; first failure retains the owned prefix.
// The final update returns void: do not invent an update result or failure stage.
// Complete reports the host-side creation/write path, not GPU sampling success.
// Borrow device/image_view; no internal cleanup, waits, submission or draw binding.
TextureBindingAttempt create_texture_binding(
    VkDevice device, VkImageView image_view,
    TextureBindingResources& resources, const TextureBindingApi& api);

// Caller guarantees no outstanding consumer references; valid device/callbacks.
// Release nonempty pool (clear pool/set), then set_layout, then sampler.
// No separate set free, no borrowed image/view/device destruction, no implicit wait.
void destroy_texture_binding(
    VkDevice device, TextureBindingResources& resources,
    const TextureBindingApi& api);
