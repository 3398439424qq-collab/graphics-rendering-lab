#pragma once
#include <vulkan/vulkan.h>

// Declarations assembled by Codex from the reviewed VK11 sections 47/49/50/53 contract.
// Function definitions are in material_binding.cpp; no resource is created here.
// Explicit unique owner; the default destructor does not release Vulkan objects.
struct MaterialBindingResources {
    MaterialBindingResources() = default;
    ~MaterialBindingResources() = default;
    MaterialBindingResources(const MaterialBindingResources&) = delete;
    MaterialBindingResources& operator=(const MaterialBindingResources&) = delete;
    MaterialBindingResources(MaterialBindingResources&&) = delete;
    MaterialBindingResources& operator=(MaterialBindingResources&&) = delete;

    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE; // Reclaimed with pool; no separate free.
};

enum class MaterialBindingStage {
    CreateSetLayout, CreatePool, AllocateSet, Complete
};

// Facts only; resources remain owned by MaterialBindingResources.
struct MaterialBindingAttempt {
    MaterialBindingStage stage = MaterialBindingStage::CreateSetLayout;
    bool complete = false;
    bool api_result_valid = false;
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
};

struct MaterialBindingApi {
    PFN_vkCreateDescriptorSetLayout create_set_layout = nullptr;
    PFN_vkCreateDescriptorPool create_pool = nullptr;
    PFN_vkAllocateDescriptorSets allocate_sets = nullptr;
    PFN_vkUpdateDescriptorSets update_sets = nullptr;
    PFN_vkDestroyDescriptorPool destroy_pool = nullptr;
    PFN_vkDestroyDescriptorSetLayout destroy_set_layout = nullptr;
};

// Preconditions: valid borrowed device/callbacks, empty owner, and a valid,
// initialized UNIFORM_BUFFER with no GPU use yet. The current VK11 buffer_info
// describes offset 0 and the 16-byte MaterialUniform range; its backing buffer
// and memory remain owned and kept alive by the caller.
// Fixed local binding 0: one ordinary UBO, fragment-visible; one pool/set.
// Record each successful output in resources. First failure returns its real
// VkResult and retains the acquired prefix; caller handles cleanup.
// update_sets returns void. Complete means host creation/write finished;
// result remains the last real allocation result, not an update result.
// Borrow buffer_info only during this call; do not retain its CPU reference.
// No buffer allocation/write/destruction, cleanup, wait, submission or binding.
MaterialBindingAttempt create_material_binding(
    VkDevice device, const VkDescriptorBufferInfo& buffer_info,
    const MaterialBindingApi& api, MaterialBindingResources& resources);

// Caller guarantees all consumers ended; valid borrowed device/callbacks.
// Destroy nonempty pool, clear pool/set, then destroy and clear set_layout.
// Empty owner is safe. No separate set free, borrowed UBO/device destruction,
// implicit wait or submission. Use nullptr allocation callbacks throughout.
void destroy_material_binding(
    VkDevice device, const MaterialBindingApi& api,
    MaterialBindingResources& resources);
