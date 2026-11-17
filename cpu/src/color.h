#pragma once
#include "vec3.h"
#include <ostream>        // ← 只要 ostream 就够了（不必拉整个 <iostream>）
#include <cmath>          // std::pow

// ── 线性颜色到 PPM 的输出边界 ───────────────────────────────────────────
// **显示编码全链只走一次**，位于线性累加/平均之后、整数化之前。
//   · 输入是线性辐射亮度估计，不应传已经 sRGB 编码的颜色。
//   · 显示编码使用标准 sRGB 传输函数，不是简单 gamma 2.2 近似。
//   · 量化为 int(255.999 * encoded)：对非负数向零截断，**不是四舍五入**。
// 重复编码会错误地抬亮中间调；两端不动点不能用于判断是否重复编码。
// 负值/非有限值输出为 0，大于 1 的有限值由 srgb 饱和到 1。
// 输出为三个 0..255 整数和换行；流由调用者持有，函数不检查写入失败。
namespace detail {
    // 标准 sRGB 传输函数（IEC 61966-2-1）：线性 [0,1] → 显示值 [0,1]
    //   分段点 0.0031308、斜率 12.92、指数 1/2.4
    inline double srgb(double v) {
        if (v <= 0.0) return 0.0;
        if (v >= 1.0) return 1.0;
        return (v <= 0.0031308) ? (12.92 * v) : (1.055 * std::pow(v, 1.0 / 2.4) - 0.055);
    }
}

// 调用者先在线性空间完成平均；这里不做采样数除法。
void write_color(std::ostream& out, const color& pixel_color) {
    // 输出端把负值和非有限值归零，避免非法显示值及非有限浮点到整数转换。
    // 这是显示端兜底；渲染阶段仍应主动报告非有限数值，避免静默输出错误颜色。
    auto clamp01 = [](double v) {
        return (std::isfinite(v) && v > 0.0) ? v : 0.0;
    };
    // 一次 sRGB 变换 + 一次量化（全链只变换一次；clamp 由 srgb() 内部完成上界）
    out << int(255.999 * detail::srgb(clamp01(pixel_color.x()))) << ' '
        << int(255.999 * detail::srgb(clamp01(pixel_color.y()))) << ' '
        << int(255.999 * detail::srgb(clamp01(pixel_color.z()))) << '\n';
}
