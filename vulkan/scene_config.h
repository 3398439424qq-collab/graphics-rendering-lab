#pragma once
#include <filesystem>
#include <string>

// Reviewed teaching declarations; parser body is learner-authored.
struct ProjectionConfig {
    float fov_y_radians = 1.57079632679f;
    float z_near = 1.0f;
    float z_far = 5.0f;
};

struct ProjectionReadResult {
    bool success = false;
    ProjectionConfig config{};
    std::string error{};
};

// Borrow text for this call only. Return owned values.
// Require each of the three named fields exactly once; order is arbitrary.
// Blank lines/outer whitespace are allowed; extra tokens/unknown keys fail.
// Failure: false, default config placeholder (do not consume), nonempty error.
// Success: true, complete finite config in the reviewed ranges, empty error.
ProjectionReadResult parse_projection_config(const std::string& text);

// Reviewed file-reading interface; implementation transcribed from learner chat.
// Borrow path for this call. Local file ownership ends before returning.
// Open/read failure follows the same default-placeholder failure contract.
// On a complete read, forward the parser result without replacing its status.
ProjectionReadResult load_projection_config(const std::filesystem::path& path);

struct ProjectionWriteResult {
    bool success = false;
    std::string error{};
};

// Borrow path/config for this call; do not modify config.
// Validate before opening: invalid config must leave the target untouched.
// A valid config directly overwrites the named target, without atomic replace.
// Preserve float round-trip precision and check the stream after explicit close.
// Failure: false/nonempty error; success: true/empty error.
ProjectionWriteResult save_projection_config(
    const std::filesystem::path& path, const ProjectionConfig& config);


struct ObjectConfig { float translation_x = 0.25f; 
    float translation_y = 0.0f; 
    float translation_z = 0.0f; 
}; 

struct MaterialConfig { 
    float tint_r = 1.0f; 
    float tint_g = 1.0f; 
    float tint_b = 1.0f; 
};

struct LightConfig {
    float direction_to_light_x = 0.0f;
    float direction_to_light_y = 0.0f;
    float direction_to_light_z = 1.0f;
    float intensity = 1.0f;
};

struct SceneConfig {
    ProjectionConfig projection{};
    ObjectConfig object{};
    MaterialConfig material{};
    LightConfig light{};
};

struct SceneReadResult {
    bool success = false;
    SceneConfig config{};
    std::string error;
};


struct SceneWriteResult {
    bool success = false;
    std::string error;
};

SceneReadResult parse_scene_config(const std::string& text);

SceneReadResult load_scene_config(const std::filesystem::path& path);

SceneWriteResult save_scene_config(
    const std::filesystem::path& path,
    const SceneConfig& config);

