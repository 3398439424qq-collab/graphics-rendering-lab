#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "scene_config.h"

// Shared fixed input, not an upload or sampling implementation.
// RGBA8 UNORM bytes, row-major: row 0 = red/green; row 1 = blue/white.
// No UV convention, image layout or resource ownership is selected here.
namespace texture_data {
inline constexpr std::uint32_t width = 2;
inline constexpr std::uint32_t height = 2;
inline constexpr std::array<std::uint8_t, 16> rgba8{
    255,   0,   0, 255,     0, 255,   0, 255,
      0,   0, 255, 255,   255, 255, 255, 255
};
inline std::array<std::uint8_t, 16>make_tinted_rgba8(const MaterialConfig& material){
    auto pixels = rgba8;
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = static_cast<std::uint8_t>(
            std::lround(pixels[i + 0] * material.tint_r));
        pixels[i + 1] = static_cast<std::uint8_t>(
            std::lround(pixels[i + 1] * material.tint_g));
        pixels[i + 2] = static_cast<std::uint8_t>(
            std::lround(pixels[i + 2] * material.tint_b));
    }
    return pixels;
}
} // namespace texture_data
