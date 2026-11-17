// Initial scaffold prepared by Codex from the reviewed VK05 contract.
#include "texture_binding.h"

TextureBindingAttempt create_texture_binding(
    VkDevice device, VkImageView image_view,
    TextureBindingResources& resources, const TextureBindingApi& api) {
    (void)device;
    (void)image_view;
    (void)resources;
    (void)api;

    VkResult res;
    TextureBindingAttempt out{};
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.pNext = nullptr;
    sampler_info.flags = 0;

    // 最近邻过滤
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    // 坐标越界时取边缘
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    // 本包只有第0级 mip
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    // 其余固定选项
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    VkSampler sampler = VK_NULL_HANDLE;
    out.stage = TextureBindingStage::CreateSampler;
    res = api.create_sampler(
        device,            // 本函数收到的device
        &sampler_info,      // 上面的配置
        nullptr,           // 使用默认内存分配器
        &sampler); // 接收新句柄
    if(res != VK_SUCCESS){
        out.api_result_valid = true;
        out.complete = false;
        out.result = res;
        return out;
    }
    resources.sampler = sampler;

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.pNext = nullptr;
    layout_info.flags = 0;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;
    VkDescriptorSetLayout created_layout = VK_NULL_HANDLE;
    out.stage = TextureBindingStage::CreateSetLayout;
    res = api.create_set_layout(
        device,
        &layout_info,
        nullptr,
        &created_layout);
    if(res != VK_SUCCESS){
        out.api_result_valid = true;
        out.complete = false;
        out.result = res;
        return out;
    }
    resources.set_layout = created_layout;  

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1; // 该类型的描述符总配额

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.pNext = nullptr;
    pool_info.flags = 0;
    pool_info.maxSets = 1;         // 最多分配1个set
    pool_info.poolSizeCount = 1;   // 下面有1种类型的配额
    pool_info.pPoolSizes = &pool_size;

    VkDescriptorPool created_pool = VK_NULL_HANDLE;

    out.stage = TextureBindingStage::CreatePool;
    res = api.create_pool(
        device,
        &pool_info,
        nullptr,
        &created_pool);
    if(res != VK_SUCCESS){
        out.api_result_valid = true;
        out.complete = false;
        out.result = res;
        return out;
    }
    resources.pool = created_pool;  

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.pNext = nullptr;
    allocate_info.descriptorPool = resources.pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &resources.set_layout;
    VkDescriptorSet allocated_set = VK_NULL_HANDLE;

    out.stage = TextureBindingStage::AllocateSet;
    res = api.allocate_sets(
        device,
        &allocate_info,
        &allocated_set);
    if(res != VK_SUCCESS){
        out.api_result_valid = true;
        out.complete = false;
        out.result = res;
        return out;
    }
    resources.set = allocated_set;

    VkDescriptorImageInfo image_info{};
    image_info.sampler = resources.sampler;
    image_info.imageView = image_view; // 本函数参数，由main传入texture_view
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstSet = resources.set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_info;
    write.pBufferInfo = nullptr;
    write.pTexelBufferView = nullptr;

    api.update_sets(device, 1, &write, 0, nullptr);

    out.complete = true;
    out.stage = TextureBindingStage::Complete;
    out.result = VK_SUCCESS;
    out.api_result_valid = true;
    return out;
}

void destroy_texture_binding(
    VkDevice device, TextureBindingResources& resources,
    const TextureBindingApi& api) {
    if(resources.pool != VK_NULL_HANDLE){
        api.destroy_pool(device,resources.pool,nullptr);
        resources.set = VK_NULL_HANDLE;
        resources.pool = VK_NULL_HANDLE;
    }
    if(resources.set_layout != VK_NULL_HANDLE){
        api.destroy_set_layout(device,resources.set_layout,nullptr);
        resources.set_layout = VK_NULL_HANDLE;
    }
    if(resources.sampler != VK_NULL_HANDLE){
        api.destroy_sampler(device,resources.sampler,nullptr);
        resources.sampler = VK_NULL_HANDLE;
    }
}
