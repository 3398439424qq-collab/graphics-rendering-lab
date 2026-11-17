#include "shader_modules.h"
#include <fstream>

SpirvReadResult read_spirv_words(const char* path){
    SpirvReadResult out{};

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if(!in) {
        out.status = SpirvReadStatus::OpenFailed;
        return out;
    }

    std::streampos end = in.tellg();
    if (end == std::streampos(-1)) {
        out.status = SpirvReadStatus::InvalidSize;
        return out;
    }

    const std::size_t byte_count = static_cast<std::size_t>(end);

    in.seekg(0,std::ios::beg);
    if (!in) {
        out.status = SpirvReadStatus::ReadFailed;
        return out;
    }

    if (byte_count == 0 || byte_count % 4 != 0) {
        out.status = SpirvReadStatus::InvalidSize;
        return out;
    }

    out.words.resize(byte_count / 4);
    in.read(reinterpret_cast<char*>(out.words.data()),
            static_cast<std::streamsize>(byte_count));
    if (!in) {
        out.words.clear();
        out.status = SpirvReadStatus::ReadFailed;
        return out;
    }

    out.status = SpirvReadStatus::Complete;
    return out;

}

ShaderModuleAttempt create_shader_module(VkDevice device,
                                         const std::vector<std::uint32_t>& words,
                                         PFN_vkCreateShaderModule create_fn){
    ShaderModuleAttempt attempt;

    VkShaderModuleCreateInfo ci{};

    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.pNext    = nullptr;
    ci.flags    = 0;
    ci.codeSize = words.size() * sizeof(std::uint32_t);
    ci.pCode    = words.data();

    attempt.result = create_fn(device, &ci, nullptr, &attempt.module);
    if(attempt.result != VK_SUCCESS){
        attempt.module = VK_NULL_HANDLE;
        return attempt;
    }
    return attempt;
}