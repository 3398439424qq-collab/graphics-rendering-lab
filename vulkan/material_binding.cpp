#include "material_binding.h"

MaterialBindingAttempt create_material_binding(
    VkDevice device, const VkDescriptorBufferInfo& buffer_info,
    const MaterialBindingApi& api, MaterialBindingResources& resources){
    MaterialBindingAttempt attempt{};

    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.pNext        = nullptr;
    layout_info.flags        = 0;
    layout_info.bindingCount = 1;
    layout_info.pBindings    = &binding;

    attempt.stage = MaterialBindingStage::CreateSetLayout;
    VkDescriptorSetLayout created_layout{};
    attempt.api_result_valid = true;
    attempt.result = api.create_set_layout(device, &layout_info, nullptr, &created_layout);
    if(attempt.result != VK_SUCCESS){
        return attempt;
    }
    resources.set_layout = created_layout;
    VkDescriptorPoolSize pool_size{};
    pool_size.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.pNext         = nullptr;
    pool_info.flags         = 0;
    pool_info.maxSets       = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes    = &pool_size;

    attempt.stage = MaterialBindingStage::CreatePool;
    VkDescriptorPool created_pool{};
    attempt.result = api.create_pool(device, &pool_info, nullptr, &created_pool);
    if(attempt.result != VK_SUCCESS){
        return attempt;
    }
    resources.pool = created_pool;

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.pNext              = nullptr;
    allocate_info.descriptorPool     = resources.pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts        = &resources.set_layout;

    attempt.stage = MaterialBindingStage::AllocateSet;
    VkDescriptorSet allocated_set{};
    attempt.result = api.allocate_sets(device, &allocate_info, &allocated_set);
    if(attempt.result != VK_SUCCESS){
        return attempt;
    }
    resources.set = allocated_set;

    VkWriteDescriptorSet write{};
    write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext            = nullptr;
    write.dstSet           = resources.set;
    write.dstBinding       = 0;
    write.dstArrayElement  = 0;
    write.descriptorCount  = 1;
    write.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pImageInfo       = nullptr;
    write.pBufferInfo      = &buffer_info;
    write.pTexelBufferView = nullptr;
    api.update_sets(device, 1, &write, 0, nullptr);

    attempt.stage = MaterialBindingStage::Complete;
    attempt.complete = true;
    return attempt;
}

void destroy_material_binding(
    VkDevice device, const MaterialBindingApi& api,
    MaterialBindingResources& resources){
    if(resources.pool != VK_NULL_HANDLE){
        api.destroy_pool(device, resources.pool, nullptr);
        resources.pool = VK_NULL_HANDLE;
        resources.set = VK_NULL_HANDLE;
    }
    if(resources.set_layout != VK_NULL_HANDLE){
        api.destroy_set_layout(device, resources.set_layout, nullptr);
        resources.set_layout = VK_NULL_HANDLE;
    }
}
