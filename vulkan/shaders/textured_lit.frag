#version 450
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec3 v_normal_world;
layout(location = 2) in vec3 v_to_camera_world;
layout(set = 0, binding = 0) uniform sampler2D texture_sampler;
layout(set = 1, binding = 0, std140) uniform MaterialBlock {
    vec4 tint_rgb_pad;
} material;
layout(push_constant, std430) uniform TransformPush {
    layout(offset = 112) vec4 light_direction_intensity;
} transform;
layout(location = 0) out vec4 outColor;

struct UnitDirectionResult {
    bool success;
    vec3 direction;
};

UnitDirectionResult make_unit_direction(vec3 value){
    if(any(isnan(value))||any(isinf(value))){
        return UnitDirectionResult(false, vec3(0.0));
    }
    vec3 abs_val = abs(value);
    float scale = max(max(abs_val.x, abs_val.y), abs_val.z);
    if (scale <= 0.0) {
        return UnitDirectionResult(false, vec3(0.0));
    }

    vec3 scaled = value / scale;
    float len = length(scaled);
    if (isnan(len) || isinf(len) || len <= 0.0) {
        return UnitDirectionResult(false, vec3(0.0));
    }

    return UnitDirectionResult(true, scaled / len);
}

float distribution_ggx(float n_dot_h, float alpha) {
    const float PI = 3.141592653589793;
    float a2 = alpha * alpha;
    float c2 = n_dot_h * n_dot_h;
    float t = (1.0 - c2) + a2 * c2;
    return a2 / (PI * t * t);
}

float geometry_smith_ggx_correlated(
    float n_dot_l, float n_dot_v, float alpha) {
    if (n_dot_l <= 0.0 || n_dot_v <= 0.0) {
        return 0.0;
    }

    float a2 = alpha * alpha;
    float p = max(n_dot_l, n_dot_v);
    float q = min(n_dot_l, n_dot_v);

    float s_p = sqrt(a2 + (1.0 - a2) * (p * p));
    float s_q = sqrt(a2 + (1.0 - a2) * (q * q));

    return (2.0 * q) / (s_q + (q / p) * s_p);
}

vec3 fresnel_schlick(float v_dot_h, vec3 f0) {
    float m = 1.0 - v_dot_h;
    float m2 = m * m;
    float m5 = m2 * m2 * m;

    return f0 + (vec3(1.0) - f0) * m5;
}

const float PI = 3.141592653589793;

vec3 shade_direct_light(
    vec3 n, vec3 l, vec3 v,
    vec3 diffuse_albedo, float alpha, vec3 f0,
    float intensity) {
    float n_dot_l = clamp(dot(n, l), 0.0, 1.0);
    float n_dot_v = clamp(dot(n, v), 0.0, 1.0);
    if (n_dot_l <= 0.0 || n_dot_v <= 0.0 || intensity <= 0.0) {
        return vec3(0.0);
    }

    vec3 h_sum = l + v;
    float h_len = length(h_sum);
    if (h_len <= 0.0) {
        return vec3(0.0);
    }
    vec3 h = h_sum / h_len;

    float n_dot_h = clamp(dot(n, h), 0.0, 1.0);
    float v_dot_h = clamp(dot(v, h), 0.0, 1.0);

    float d = distribution_ggx(n_dot_h, alpha);
    float g = geometry_smith_ggx_correlated(n_dot_l, n_dot_v, alpha);
    vec3 f = fresnel_schlick(v_dot_h, f0);

    vec3 diffuse = (vec3(1.0) - f) * (diffuse_albedo * n_dot_l / PI);
    vec3 specular = (g / (4.0 * n_dot_v)) * d * f;

    return intensity * (diffuse + specular);
}

void main(){
    vec4 base_color = texture(texture_sampler, v_uv);
    UnitDirectionResult n = make_unit_direction(v_normal_world);
    UnitDirectionResult v = make_unit_direction(v_to_camera_world);
    if(!n.success||!v.success){
        outColor = vec4(0.0, 0.0, 0.0, base_color.a);
        return;
    }
    vec3 lit_rgb = shade_direct_light(
        n.direction,
        transform.light_direction_intensity.xyz,
        v.direction,
        base_color.rgb * material.tint_rgb_pad.xyz,
        1.0,
        vec3(0.04),
        transform.light_direction_intensity.w
    );
    outColor = vec4(lit_rgb, base_color.a);
}
