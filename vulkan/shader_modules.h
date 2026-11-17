// 着色器模块：把 SPIR-V 文件读成 32 位字数组，并据此创建 VkShaderModule。
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

enum class SpirvReadStatus { Complete, OpenFailed, InvalidSize, ReadFailed };

struct SpirvReadResult {
    SpirvReadStatus status = SpirvReadStatus::OpenFailed;
    std::vector<std::uint32_t> words;
};

struct ShaderModuleAttempt {
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    VkShaderModule module = VK_NULL_HANDLE;
};

SpirvReadResult read_spirv_words(const char* path);

ShaderModuleAttempt create_shader_module(VkDevice device,
                                         const std::vector<std::uint32_t>& words,
                                         PFN_vkCreateShaderModule create_fn);
