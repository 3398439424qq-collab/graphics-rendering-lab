// ── src/texture.h ─────────────────────────────────────────
// 纹理抽象：虚函数 texel() 读取纹素，非虚 value() 统一执行双线性采样。
// 颜色均在线性空间；构造后只读，const 调用不修改纹素，派生类须遵守该约定。
#pragma once
#include "color.h"
#include "fatal.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

class texture {
  public:
    virtual ~texture() = default;

    // ── value(u,v) ──
    // 前置：u,v 有限且 ∈ [0,1]；size() >= 1 且稳定；坐标乘 N 的 floor 结果须能表示为 int
    // 后置：N>1 时以 u*N、v*N 的整数格及小数权重作双线性采样：4 次 texel() ＋ 3 次 lerp
    //       调用者保证纹素颜色有限、插值运算不溢出；此处不作数值检查
    // 边界：u 或 v 为 1 时索引钳到 N-1；末格相邻索引重复边缘纹素，不环绕，也不修正负 UV
    // 成本：N==1 时直接返回 texel(0,0)，只取一次、不插值；此时返回色的有限性与该纹素一致
    //       采样返回线性颜色，不做 gamma/sRGB 变换；UV 不满足前置时没有运行期保护
    // 类内定义隐式 inline，可从多个翻译单元包含
    color value(double u, double v) const {
        const int N = size();
        if (N == 1) return texel(0, 0);

        // 整数格（u=1.0 时 i_raw 可达 N）
        const int i_raw = static_cast<int>(std::floor(u * N));
        const int j_raw = static_cast<int>(std::floor(v * N));

        // 钳到合法纹素索引
        const int i0 = std::min(i_raw, N - 1);
        const int j0 = std::min(j_raw, N - 1);
        const int i1 = std::min(i0 + 1, N - 1);
        const int j1 = std::min(j0 + 1, N - 1);

        // 权重 = u*N / v*N 的小数部分，∈ [0,1)
        const double fu = u * N - i_raw;
        const double fv = v * N - j_raw;

        const color c00 = texel(i0, j0);
        const color c10 = texel(i1, j0);
        const color c01 = texel(i0, j1);
        const color c11 = texel(i1, j1);

        const color cx0 = (1.0 - fu) * c00 + fu * c10;
        const color cx1 = (1.0 - fu) * c01 + fu * c11;
        return (1.0 - fv) * cx0 + fv * cx1;
    }

    // ── texel(i,j) ──
    // 前置：i,j 为整数，且 ∈ [0, size())
    // 后置：返回第 (i,j) 个纹素的颜色
    // 越界：违反接口前置；此处不检查，具体后果依派生类实现（checker 的数组访问可能越界）
    virtual color texel(int i, int j) const = 0;

    // ── size() ──
    // 后置：返回稳定的每维纹素数 N，且 N >= 1；派生类须保证尺寸与纹素存储一致
    //       基类 value() 需要它算索引 ⇒ 必须能拿到，故留在基类。
    virtual int size() const = 0;
};

class solid_color : public texture {
  public:
    explicit solid_color(const color& c):c_{c}{}
    // 忽略 i,j，恒返回 c_（前置同基类）
    color texel([[maybe_unused]] int i,[[maybe_unused]] int j) const override{return c_;}
    // 常量纹理没有数组 ⇒ 返回 1（size()==1 ⇒ value() 走早返回，只取 1 次 texel()）
    int size() const override{return 1;}
  private:
    color c_;
};

class checker_texture : public texture {
  public:
    explicit checker_texture(int n):n_{n}{
    if (n_ < 1) {
            FATAL("checker_texture: n must be >= 1, got " + std::to_string(n_));
        }

        // 检查之后再分配：避免用负 n 先构造出一个巨大的 size_t（或者 n==0 时空表）
        // 才在下一步报错——错误信息指向真正的输入，而不是 vector 的分配失败。
        texels_.resize(static_cast<std::size_t>(n_) * static_cast<std::size_t>(n_));

        for (int j = 0; j < n_; ++j) {
            for (int i = 0; i < n_; ++i) {
                const bool dark = ((i / 2 + j / 2) % 2 == 0);
                texels_[j * n_ + i] = dark ? color(0.1, 0.1, 0.1)
                                           : color(0.9, 0.9, 0.9);
            }
        }
    }
          
    // 第 (i,j) 个纹素（前置同基类）；颜色由构造期按下面的棋盘规则生成
    color texel(int i, int j) const override{return texels_[j * n_ + i];}
    int   size() const override{return n_;}
  private:
    int n_;
    // n*n，行优先：texels_[j*n + i]，构造期【生成一次】
    // 生成规则：每个同色块为 2×2 纹素；深色 ⟺ ((i/2 + j/2) % 2 == 0)
    //   · 沿每轴完整深浅周期为 4 纹素；有限 N 可截断末块，value() 在分界附近作双线性过渡
    //   · ((i + j) & 1) 则每纹素交替、完整周期为 2，频率更高，插值可能混成近灰
    std::vector<color> texels_;
};
