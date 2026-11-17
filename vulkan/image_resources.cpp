#include "image_resources.h"
#include "buffer_resources.h"  // Reuse select_memory_type.
#include <utility>  // std::move used by the learner's ownership transfers.

// VK05 implementation entry prepared by Codex; function bodies belong to the learner.
// This file is intentionally incomplete and is not yet in PROD_SOURCES.

// 1. Define destroy_image_allocation here (declaration in image_resources.h).
//    Contract: release owned resources and clear handles; see the header.

// 2. Define create_image_allocation here (declaration in image_resources.h).
//    Contract and fixed VkImageCreateInfo fields: VK05 task document, section 8.
//    Related example: buffer_resources.cpp, create_buffer_allocation.

void destroy_image_allocation(VkDevice device,ImageAllocation& image,const ImageResourcesApi& image_api){
    if(image.image != VK_NULL_HANDLE){
        image_api.destroy_image(device,image.image,nullptr);
        image.image = VK_NULL_HANDLE;
    }
    if(image.memory != VK_NULL_HANDLE){
        image_api.free_memory(device,image.memory,nullptr);
        image.memory = VK_NULL_HANDLE;
    }
}

ImageAllocationAttempt create_image_allocation(VkDevice device,const ImageAllocationConfig& image_config,
    ImageAllocation& image,const ImageResourcesApi& image_api){
    ImageAllocationAttempt out{};  
    VkImageCreateInfo info{};
    info.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;

    // 这三项来自调用者提供的配置
    info.extent = image_config.extent;
    info.format = image_config.format;
    info.usage  = image_config.usage;

    // 本包固定的简单图像配置
    info.mipLevels   = 1;                       // 只有原始分辨率这一层级
    info.arrayLayers = 1;                       // 一层图像
    info.samples     = VK_SAMPLE_COUNT_1_BIT;   // 不使用多重采样
    info.tiling      = VK_IMAGE_TILING_OPTIMAL; // 适合GPU访问的存储排列
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult res;
    VkImage created_image = VK_NULL_HANDLE;
    res = image_api.create_image(device,&info,nullptr,&created_image);
    out.stage   = ImageAllocationStage::CreateImage;
    if (res != VK_SUCCESS) { 
        out.complete = false;
        out.result = res;
        out.api_result_valid = true;
        return out;
    }
    image.image = created_image;

    out.stage = ImageAllocationStage::SelectMemoryType;
    VkMemoryRequirements req{};
    image_api.get_image_memory_requirements(
        device, image.image, &req);

    auto type_index = select_memory_type(
        image_config.mem_props,
        req.memoryTypeBits,
        image_config.required_flags);

    if (!type_index) {
        out.complete = false;
        out.api_result_valid = false;
        return out;
    }

    out.stage = ImageAllocationStage::AllocateMemory;
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = *type_index;
    VkDeviceMemory allocated_memory{};
    res = image_api.allocate_memory(
        device,             // 所属设备
        &alloc_info,        // 输入：分配描述
        nullptr,            // 默认主机内存分配回调
        &allocated_memory); // 输出：分配得到的内存句柄
    if (res != VK_SUCCESS) { 
        out.complete = false;
        out.result = res;
        out.api_result_valid = true;
        return out;
    }
    image.memory = allocated_memory;

    out.stage = ImageAllocationStage::BindImageMemory;
    res = image_api.bind_image_memory(device,image.image,image.memory,0);
    if (res != VK_SUCCESS) { 
        out.complete = false;
        out.result = res;
        out.api_result_valid = true;
        return out;
    }

    out.complete = true;
    out.result = res;
    out.api_result_valid = true;
    out.stage = ImageAllocationStage::Complete;
    return out;
}

void TextureImageOwner::reset() noexcept {
    destroy_image_allocation(device,TextureImageOwner::allocation,TextureImageOwner::image_api);
    device = VK_NULL_HANDLE;
    image_api = ImageResourcesApi{};
}

TextureImageOwner::~TextureImageOwner() noexcept {
    reset();
}

void TextureImageOwner::release_to(
    ImageAllocation& destination) noexcept {
    destination.image = std::move(allocation.image);
    destination.memory = std::move(allocation.memory);
    allocation.image = VK_NULL_HANDLE;
    allocation.memory = VK_NULL_HANDLE;
    device = VK_NULL_HANDLE;
    image_api = ImageResourcesApi{};
}

TextureImageOwner::TextureImageOwner(
    TextureImageOwner&& other) noexcept {
    device = other.device;
    image_api = other.image_api;
    other.release_to(allocation);
}

TextureImageOwner& TextureImageOwner::operator=(
    TextureImageOwner&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    reset();
    device = other.device;
    image_api = other.image_api;
    other.release_to(allocation);
    return *this;
}