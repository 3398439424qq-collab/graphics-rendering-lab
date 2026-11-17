#version 450
layout(location = 1) in vec3 v_normal_world;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 n_world = normalize(v_normal_world);
    vec3 rgb = n_world * 0.5 + vec3(0.5);
    outColor = vec4(rgb, 1.0);
}