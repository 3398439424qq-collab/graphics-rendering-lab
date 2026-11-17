#include "scene_config.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

// Learner chat submission, transcribed by Codex on 2026-11-13.
// Applies the learner's explicit FOV-name and local-valid corrections.
// Stream-reading scaffold and reviewed type declarations were AI-provided.
ProjectionReadResult parse_projection_config(const std::string& text)
{
    ProjectionConfig candidate{};
    bool seen_fov = false;
    bool seen_near = false;
    bool seen_far = false;

    std::istringstream input(text);
    std::string line;

    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string key;

        if (!(row >> key)) {
            continue;
        }

        float value{};
        if (!(row >> value)) {
            return {false, {}, "missing or invalid value"};
        }

        std::string extra;
        if (row >> extra) {
            return {false, {}, "unexpected trailing content"};
        }

        if (key == "fov_y_radians") {
            if (seen_fov) {
                return {false, {}, "duplicate key: fov"};
            }
            candidate.fov_y_radians = value;
            seen_fov = true;
        } else if (key == "z_near") {
            if (seen_near) {
                return {false, {}, "duplicate key: z_near"};
            }
            candidate.z_near = value;
            seen_near = true;
        } else if (key == "z_far") {
            if (seen_far) {
                return {false, {}, "duplicate key: z_far"};
            }
            candidate.z_far = value;
            seen_far = true;
        } else {
            return {false, {}, "unknown key: " + key};
        }
    }

    if (!seen_fov) {
        return {false, {}, "missing field: fov"};
    }
    if (!seen_near) {
        return {false, {}, "missing field: z_near"};
    }
    if (!seen_far) {
        return {false, {}, "missing field: z_far"};
    }

    const float pi = 3.14159265358979323846f;
    const bool valid = std::isfinite(candidate.fov_y_radians)
        && std::isfinite(candidate.z_near)
        && std::isfinite(candidate.z_far)
        && candidate.fov_y_radians > 0
        && candidate.fov_y_radians < pi
        && candidate.z_near > 0
        && candidate.z_far > candidate.z_near;
    if (!valid) {
        return {false, {}, "invalid projection config"};
    }

    return {true, candidate, {}};
}

// Learner first submission, transcribed by Codex on 2026-11-13.
ProjectionReadResult load_projection_config(
    const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) return {false, {}, "error"};

    std::string text;
    std::string line;
    while (std::getline(input, line)) {
        text += line;
        text += '\n';
    }

    if (input.bad() || !input.eof()) return {false, {}, "error"};

    return parse_projection_config(text);
}

// Learner first submission, transcribed by Codex on 2026-11-13.
ProjectionWriteResult save_projection_config(
    const std::filesystem::path& path,
    const ProjectionConfig& config)
{
    constexpr float kPi = 3.14159265358979323846f;
    if (!std::isfinite(config.fov_y_radians) ||
        !std::isfinite(config.z_near) ||
        !std::isfinite(config.z_far)) {
        return {false, "projection config values must be finite"};
    }
    if (config.fov_y_radians <= 0.0f || config.fov_y_radians >= kPi) {
        return {false, "fov_y_radians must be strictly between 0 and pi"};
    }
    if (config.z_near <= 0.0f) {
        return {false, "z_near must be positive"};
    }
    if (config.z_far <= config.z_near) {
        return {false, "z_far must be greater than z_near"};
    }

    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        return {false, "failed to open projection config file for writing"};
    }

    output << std::setprecision(
        std::numeric_limits<float>::max_digits10);
    output << "fov_y_radians " << config.fov_y_radians << '\n';
    output << "z_near " << config.z_near << '\n';
    output << "z_far " << config.z_far << '\n';

    output.close();
    if (!output) {
        return {false, "failed to write projection config file"};
    }

    return {true, {}};
}

SceneReadResult load_scene_config(
    const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {false, {}, "failed to open scene config file"};
    }

    std::string text;
    std::string line;
    while (std::getline(input, line)) {
        text += line;
        text += '\n';
    }

    if (input.bad() || !input.eof()) {
        return {false, {}, "failed to read scene config file"};
    }

    return parse_scene_config(text);
}

SceneReadResult parse_scene_config(const std::string& text)
{
    SceneConfig candidate{};
    bool seen_x = false, seen_y = false, seen_z = false;
    bool seen_r = false, seen_g = false, seen_b = false;
    bool seen_lx = false, seen_ly = false, seen_lz = false, seen_intensity = false;
    const std::string prefix = "projection.";
    std::string projection_text;
    std::istringstream input(text);
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream row(line);
        std::string key;
        if (!(row >> key)) {
            continue;
        }

        if (key.compare(0, prefix.size(), prefix) == 0) {
            std::string rest;
            std::getline(row, rest);
            projection_text += key.substr(prefix.size());
            projection_text += rest;
            projection_text += '\n';
            continue;
        }

        float value = 0.0f;
        if (!(row >> value) || !std::isfinite(value)) {
            return {false, {}, "invalid scene field value"};
        }

        std::string extra;
        if (row >> extra) {
            return {false, {}, "unexpected trailing content on scene line"};
        }

        if (key == "object.translation_x") {
            if (seen_x) {
                return {false, {}, "duplicate object.translation_x"};
            }
            candidate.object.translation_x = value;
            seen_x = true;
        } else if (key == "object.translation_y") {
            if (seen_y) {
                return {false, {}, "duplicate object.translation_y"};
            }
            candidate.object.translation_y = value;
            seen_y = true;
        } else if (key == "object.translation_z") {
            if (seen_z) {
                return {false, {}, "duplicate object.translation_z"};
            }
            candidate.object.translation_z = value;
            seen_z = true;
        } else if (key == "material.tint_r") {
            if (seen_r) {
                return {false, {}, "duplicate material.tint_r"};
            }
            if (value < 0.0f || value > 1.0f) {
                return {false, {}, "material.tint_r must be between 0 and 1"};
            }
            candidate.material.tint_r = value;
            seen_r = true;
        } else if (key == "material.tint_g") {
            if (seen_g) {
                return {false, {}, "duplicate material.tint_g"};
            }
            if (value < 0.0f || value > 1.0f) {
                return {false, {}, "material.tint_g must be between 0 and 1"};
            }
            candidate.material.tint_g = value;
            seen_g = true;
        } else if (key == "material.tint_b") {
            if (seen_b) {
                return {false, {}, "duplicate material.tint_b"};
            }
            if (value < 0.0f || value > 1.0f) {
                return {false, {}, "material.tint_b must be between 0 and 1"};
            }
            candidate.material.tint_b = value;
            seen_b = true;
        } else if (key == "light.direction_to_light_x") {
            if (seen_lx) {
                return {false, {}, "duplicate light.direction_to_light_x"};
            }
            candidate.light.direction_to_light_x = value;
            seen_lx = true;
        } else if (key == "light.direction_to_light_y") {
            if (seen_ly) {
                return {false, {}, "duplicate light.direction_to_light_y"};
            }
            candidate.light.direction_to_light_y = value;
            seen_ly = true;
        } else if (key == "light.direction_to_light_z") {
            if (seen_lz) {
                return {false, {}, "duplicate light.direction_to_light_z"};
            }
            candidate.light.direction_to_light_z = value;
            seen_lz = true;
        } else if (key == "light.intensity") {
            if (seen_intensity) {
                return {false, {}, "duplicate light.intensity"};
            }
            if (value < 0.0f || value > 1.0f) {
                return {false, {}, "light.intensity must be between 0 and 1"};
            }
            candidate.light.intensity = value;
            seen_intensity = true;
        } else {
            return {false, {}, "unknown scene config key: " + key};
        }
    }

    if (!seen_x || !seen_y || !seen_z) {
        return {false, {}, "missing required object translation field"};
    }

    if ((seen_r || seen_g || seen_b) && !(seen_r && seen_g && seen_b)) {
        return {false, {}, "incomplete material tint fields"};
    }

    const bool any_light = seen_lx || seen_ly || seen_lz || seen_intensity;
    const bool all_light = seen_lx && seen_ly && seen_lz && seen_intensity;
    if (any_light && !all_light) {
        return {false, {}, "incomplete light fields"};
    }

    if (candidate.light.direction_to_light_x == 0.0f &&
        candidate.light.direction_to_light_y == 0.0f &&
        candidate.light.direction_to_light_z == 0.0f) {
        return {false, {}, "light direction must be non-zero"};
    }

    ProjectionReadResult projection_result =
        parse_projection_config(projection_text);
    if (!projection_result.success) {
        return {false, {}, projection_result.error};
    }

    candidate.projection = projection_result.config;
    return {true, candidate, {}};
}

SceneWriteResult save_scene_config(
    const std::filesystem::path& path,
    const SceneConfig& config)
{
    std::ostringstream encoded;
    encoded << std::setprecision(std::numeric_limits<float>::max_digits10);
    encoded << "projection.fov_y_radians "
            << config.projection.fov_y_radians << '\n'
            << "projection.z_near "
            << config.projection.z_near << '\n'
            << "projection.z_far "
            << config.projection.z_far << '\n'
            << "object.translation_x "
            << config.object.translation_x << '\n'
            << "object.translation_y "
            << config.object.translation_y << '\n'
            << "object.translation_z "
            << config.object.translation_z << '\n'
            << "material.tint_r "
            << config.material.tint_r << '\n'
            << "material.tint_g "
            << config.material.tint_g << '\n'
            << "material.tint_b "
            << config.material.tint_b << '\n'
            << "light.direction_to_light_x "
            << config.light.direction_to_light_x << '\n'
            << "light.direction_to_light_y "
            << config.light.direction_to_light_y << '\n'
            << "light.direction_to_light_z "
            << config.light.direction_to_light_z << '\n'
            << "light.intensity "
            << config.light.intensity << '\n';

    if (!encoded) return {false, "failed to encode scene config"};
    const std::string text = encoded.str();
    SceneReadResult result = parse_scene_config(text);
    if (!result.success) return {false, result.error};

    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) return {false, "failed to open scene config"};
    output << text;
    output.close();
    if (!output) return {false, "failed to write scene config"};
    return {true, {}};
}