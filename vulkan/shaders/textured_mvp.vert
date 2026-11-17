#version 450

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec3 v_normal_world;
layout(location = 2) out vec3 v_to_camera_world;

layout(push_constant, std430) uniform TransformPush {
    layout(offset = 0)  mat4 mvp;
    layout(offset = 64) vec4 normal_col0_camera_x;
    layout(offset = 80) vec4 normal_col1_camera_y;
    layout(offset = 96) vec4 normal_col2_camera_z;
} transform;

void main() {
    mat3 normal_matrix = mat3(
        transform.normal_col0_camera_x.xyz,
        transform.normal_col1_camera_y.xyz,
        transform.normal_col2_camera_z.xyz
    );
    v_normal_world = normal_matrix * a_normal;

    vec3 q = vec3(
        transform.normal_col0_camera_x.w,
        transform.normal_col1_camera_y.w,
        transform.normal_col2_camera_z.w
    );
    v_to_camera_world = q - a_position;

    gl_Position = transform.mvp * vec4(a_position, 1.0);
    v_uv = a_uv;
}
