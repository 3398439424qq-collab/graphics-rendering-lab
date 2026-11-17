#version 450

// Codex interface scaffold; learner writes the sampling output.
layout(location = 0) in vec2 v_uv;
layout(set = 0, binding = 0) uniform sampler2D texture_sampler;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texture_sampler, v_uv);
}
