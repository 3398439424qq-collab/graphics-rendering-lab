#pragma once
#include "color.h"
#include "ray_hand.h"
#include "hittable.h"
#include "texture.h"
#include <memory>
// ── material.h ─────────────────────────────────────────────
class material {
  public:
    virtual ~material() = default;
    material(const material&) = delete;
    material& operator=(const material&) = delete;
    material() = default;
    material(material&&) = default;
    material& operator=(material&&) = default;

    // 前置：rec.normal 单位且朝入射侧；r_in.direction() 单位；u1,u2 ∈ [0,1)
    // 后置：true ⇒ attenuation 为路径乘权（不另乘余弦）、scattered 原点=rec.p；半球约束由具体材质决定
    //       false ⇒ 结束散射，保留已累加的 emission，不使用散射出参；折射方向不受反射半球限制
    // 禁止：修改 rec、读写任何全局/静态 RNG（散射随机输入由调用方给出 u1,u2）
    virtual bool scatter([[maybe_unused]] const ray& r_in, const hit_record& rec,
                         color& attenuation, ray& scattered,
                         double u1, double u2) const = 0;
    // 返回命中处的线性自发光色，默认黑色；调用方在 scatter 前累加。
    // front_face 表示入射命中正面；u/v/p 供具体发光材质定位命中点。
    virtual color emitted([[maybe_unused]] double u,[[maybe_unused]] double v,[[maybe_unused]] const point3& p,[[maybe_unused]] bool front_face) const{
      return color(0,0,0);
    }
    // 供场景检测是否含发光材质，不保证每个命中点都发光。
    virtual bool is_emissive() const noexcept { return false; }
};

// 从单位法线构造切向基（Frisvad 方法）；接近 -z 时特判，避免分母 1+n.z() 接近零。
inline void basis_branchless(const vec3& n, vec3& b1, vec3& b2) {
    if (n.z() < -0.9999999) { b1 = vec3(0,-1,0); b2 = vec3(-1,0,0); return; }
    const double a = 1.0 / (1.0 + n.z());
    const double b = -n.x() * n.y() * a;
    b1 = vec3(1.0 - n.x()*n.x()*a,  b,                     -n.x());
    b2 = vec3(b,                    1.0 - n.y()*n.y()*a,   -n.y());
}

// 漫反射：在法线半球内按余弦加权采样，路径乘权等于纹理反射率。
// 共享持有只读纹理；调用者必须提供非空纹理以及满足纹理契约的 UV。
class lambertian : public material {
  public:
    explicit lambertian(const color& a) : lambertian(std::make_shared<solid_color>(a)) {}

    explicit lambertian(std::shared_ptr<const texture> a)
        : albedo_{std::move(a)} {}
        
    bool scatter([[maybe_unused]] const ray& r_in, const hit_record& rec,
                 color& attenuation, ray& scattered,
                 double u1, double u2) const override{
        constexpr double TWO_PI = 6.283185307179586; 
        const double r  = std::sqrt(u1);
        const double phi = TWO_PI*u2;
        double lx = r*std::cos(phi), ly = r*std::sin(phi), lz = std::sqrt(1.0 - u1);
        vec3 b1, b2;
        basis_branchless(rec.normal, b1, b2);

        // 将局部余弦加权方向变换到世界坐标
        const vec3 dir = unit_vector(lx * b1 + ly * b2 + lz * rec.normal);

        // 输出方向与路径乘权
        attenuation = albedo_->value(rec.u, rec.v);// 余弦加权采样下正好抵消 π 和 cosθ
        scattered   = ray(rec.p, dir);
        return true;       
        }
  private:
    std::shared_ptr<const texture> albedo_;
};

// ── 金属：镜面反射 ＋ 粗糙近似 ────────────────────────────────
// 前置：fuzz 有限且在 [0,1]；它是归一化的【模糊尺度】，不是物理粗糙度（构造器未检查）
//   · 偏移角上限 ≈ asin(fuzz)；fuzz=0 退化为完美镜面
//   · 与 GGX/Trowbridge-Reitz 的 roughness 不是同一个量（分布不同、不可换算）
//   ⚠️ 本实现是 RTOW 的【粗糙近似】，不是 GGX PBR；不得表述为"已实现 PBR"
class metal : public material {
  public:
    metal(const color& a, double f) : albedo{a}, fuzz{f} {}

    bool scatter([[maybe_unused]] const ray& r_in, const hit_record& rec,
                 color& attenuation, ray& scattered,
                 double u1, double u2) const override {
        constexpr double TWO_PI = 6.283185307179586;   // 与 lambertian 同一常量

        // ① 完美反射方向（rec.normal 单位且朝入射侧 ⇒ 结果指向入射侧半球）
        const vec3 rfl = r_in.direction() - 2.0*dot(r_in.direction(), rec.normal)*rec.normal;

        // ② 球面偏移近似：拒绝表面下方方向，至多尝试 8 轮；不是微表面 BRDF 采样
        //    · 每轮由 u1,u2 加固定增量并取模派生；候选相关，确定可复现，不等同独立重抽
        //    · 截断后失败会吸收；未作对应 PDF/能量补偿，不据此保证无偏或重试一定成功
        vec3 dir;
        bool ok = false;
        for (int attempt = 0; attempt < 8; ++attempt) {
            const double a1 = std::fmod(u1 + 0.6180339887498949 * (attempt + 1), 1.0);
            const double a2 = std::fmod(u2 + 0.7548776662466927 * (attempt + 1), 1.0);
            const double theta = std::acos(1.0 - 2.0*a1);      // acos ⇒ 球面均匀
            const double phi   = TWO_PI * a2;
            const vec3 offset(std::sin(theta)*std::cos(phi),
                              std::sin(theta)*std::sin(phi),
                              std::cos(theta));
            dir = unit_vector(rfl + fuzz*offset);
            if (dot(dir, rec.normal) > 0.0) { ok = true; break; }
        }
        if (!ok) return false;                 // 8 轮均未接受：结束散射，不承诺失败概率

        attenuation = albedo;                  // 反射率（金属：只衰减、不发光）
        scattered   = ray(rec.p, dir);         // 起点必须是命中点
        return true;
    }

  private:
    color  albedo;
    double fuzz;
};

// ── 介质：折射 ＋ Schlick 反射选择 ＋ 全反射 ────────────────────
// 简化的无吸收界面模型：一次散射只选择一条反射或折射方向。
//   · 漫反射 / 金属的路径乘权来自 albedo；介质的分支乘权由能量系数与选择概率之比决定
//   · 介质按 R 与 1−R 的概率取支，不把对应能量系数再乘一次
// 模型范围：
//   · 反射率用 **Schlick 近似** R = R0 + (1−R0)(1−cosθi)^5，
//     **不是精确 Fresnel**，也未处理偏振
//   · 本实现**不含色散**、**不含 Beer–Lambert 吸收**，未另计辐射传输的折射率平方因子
//   · 全反射（sin²θt > 1）时必选反射、权重为 1；仅在折射率比 η > 1 时可能出现
// 前置：ir 有限且为正；单位入射方向与朝入射侧的单位法线给出 c := |d·n| = cosθi
//       front_face 为 true 用 η = 1/ir，否则 η = ir；只表达空气与本介质，不追踪嵌套介质
class dielectric : public material {
  public:
    explicit dielectric(double index_of_refraction) : ir{index_of_refraction} {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation,
                 ray& scattered, double u1, [[maybe_unused]] double u2) const override {
        vec3 d = r_in.direction();
        vec3 n = rec.normal;
        const double c = std::min(std::abs(dot(d,n)),1.0);
        const double eta = rec.front_face?1/ir:ir;
        const double sin_theta_t = std::sqrt(1-c*c)*eta;
        double weight = 0;
        if(sin_theta_t > 1){
            weight = 1;
            vec3 rfl = d - 2*dot(d,n)*n;
            scattered = ray(rec.p,rfl);
            attenuation = color(weight,weight,weight);
            return true;
        }

        double cos_theta_t = std::sqrt(1-sin_theta_t*sin_theta_t);
        const double a = (1.0-eta)/(1.0+eta);
        const double R_0 = a*a;
        const double m = 1- c;
        const double R = R_0 + (1-R_0)*m*m*m*m*m;

        
        // ── 分支估计：能量系数与选择概率相消 ──────────────────
        // p(反射)=R、p(折射)=1−R；在被选中的非零概率分支，权重 = 该支能量系数 / 该支概率 = 1
        //   因此平均贡献为 R·L_反 + (1−R)·L_折，针对上述简化界面模型
        // [历史锚点·勿删] 以 p(反射)=R、p(折射)=1−R 取支后，又分别乘 R 或 1−R 的旧设计已被白炉测试否证。
        // 旧期望贡献为 R²·L_反 + (1−R)²·L_折，重复乘分支系数导致偏暗；本模型须保留权重 1。
        // 历史证据（仓库根相对路径）：笔记/2026-09-28-V4.md §四（两版权重的白炉对照）。
        //   这里权重恒为 1 不等于完整物理传输：折射率平方因子、体吸收等并未纳入
        //   常数背景下路径仍可能受深度截断影响，不能据权重推出玻璃一定不可见
        //   本选择只用 u1；u2 保留在统一散射接口中。
        if (u1 < R) {
            // 反射支（概率 R ⇒ 权重 1）
            vec3 rfl = d - 2.0 * dot(d, n) * n;
            scattered = ray(rec.p, rfl);
            weight=1.0;
            attenuation = color(weight,weight,weight);
        } else {
            // 折射支（概率 1−R ⇒ 权重 1）
            weight=1.0;
            vec3 dir = eta*d+(eta*c - cos_theta_t)*n;   // 确定性方向
            scattered = ray(rec.p, dir);
            attenuation = color(weight,weight,weight);
        }
        
        return true;
    }

  private:
    double ir;   
};

// 单面光源：正面返回 L_，背面黑色；不散射，命中发光后终止光路。
class diffuse_light : public material {
public:
    explicit diffuse_light(const color& L) : L_{L} {}

    color emitted([[maybe_unused]] double u,
                  [[maybe_unused]] double v,
                  [[maybe_unused]] const point3& p,
                  bool front_face) const override {
        return front_face ? L_ : color(0, 0, 0);
    }

    bool scatter([[maybe_unused]] const ray& r_in,
                 [[maybe_unused]] const hit_record& rec,
                 [[maybe_unused]] color& attenuation,
                 [[maybe_unused]] ray& scattered,
                 [[maybe_unused]] double u1,
                 [[maybe_unused]] double u2) const override {
        return false;
    }

    bool is_emissive() const noexcept override{return true;}

private:
    color L_;
};
