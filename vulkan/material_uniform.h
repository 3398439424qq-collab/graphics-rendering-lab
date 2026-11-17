#pragma once
#include <array>
#include "scene_config.h"

struct alignas(16) MaterialUniform { 
    std::array<float, 4> tint_rgb_pad{}; 

}; 

inline MaterialUniform make_material_uniform(const MaterialConfig& material) {
    MaterialUniform result{};

    result.tint_rgb_pad[0] = material.tint_r;
    result.tint_rgb_pad[1] = material.tint_g;
    result.tint_rgb_pad[2] = material.tint_b;
    result.tint_rgb_pad[3] = 0.0f;

    return result;
}