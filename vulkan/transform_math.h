#pragma once
#include <cmath>
#include <array>


inline std::array<float, 16> multiply_mat4(
    const std::array<float, 16>& a,
    const std::array<float, 16>& b)
{
    std::array<float,16> v{};
    for(int i = 0 ;i<16;i++){
        const int row     = i % 4; 
        const int colBase = i - row;      
        for (int k = 0; k < 4; ++k) {
            v[i] += a[row + 4*k] * b[k + colBase];   
        }
    }
    return v;
}

inline std::array<float, 16> identity_mat4() {
    std::array<float, 16> m{};
    m[0]  = 1.0f;
    m[5]  = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
    return m;
}
inline std::array<float, 16> translation_mat4(float tx, float ty, float tz) {
    std::array<float, 16> m = identity_mat4();
    m[12] = tx;
    m[13] = ty;
    m[14] = tz;
    return m;
}

inline std::array<float, 16> perspective_vulkan_mat4(
    float fov_y_radians, float aspect,
    float z_near, float z_far){
        std::array<float, 16> p{};
        const float y_scale = 1.0f / std::tan(fov_y_radians * 0.5f);
        const float x_scale = y_scale / aspect;
        p[0] = x_scale;
        p[5] = -y_scale;
        p[10] = z_far / (z_near - z_far);
        p[11] = -1.0f;
        p[14] = (z_near * z_far) / (z_near - z_far);
        return p;
    }

inline std::array<float, 16> scale_mat4(float sx,float sy,float sz){
    auto m = identity_mat4();
    m[0] = sx;
    m[5] = sy;
    m[10] = sz;
    return m;
}