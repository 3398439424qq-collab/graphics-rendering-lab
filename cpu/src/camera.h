#pragma once
#include "ray_hand.h"
#include "vec3.h"
#include <cmath>

// ── camera：针孔射线与可移动正交基 ──────────────────────────────────────────
// 调用者保证输入与中间运算有限，aspect/vh/focal > 0；FOV 工厂要求 0 < vfov_deg < 180。
// lookat 必须不同于 lookfrom，up_hint 可为零（会选回退轴）；这些前置不做运行检查。
// 默认基为 right=(1,0,0)、up=(0,1,0)、forward=(0,0,-1)，相机中心在原点。
// [历史锚点·勿删] 默认路径（上述原点/基、focal=1）须与旧相机逐位相同，以守住 5 张锚点 md5 不变。
// 契约（仓库根相对路径）：练习/复习/冷启动/CPU-06_相机/README.md §四 K-h；不能仅以方向近似相等替代逐位检查。
// **尺度约定**：偏移直接按物理视口尺寸缩放：sx=(u-0.5)*aspect*vh、sy=(v-0.5)*vh（不含 focal）。
// vh 是距相机中心 focal 处视口的**物理高度**；FOV 工厂取 vh = 2*tan(vfov/2)*focal（与 focal 成正比），
// 因而对任意 focal > 0 都保持标准垂直视场；直接构造固定 vh 时不保证固定视场角。
// from_lookat 系列将相机中心移到 lookfrom，并把 forward 指向 lookat。
// 基构造：r=unit(cross(f,up_ref))，u=cross(r,f)，使 cross(r,u)=-f。
// 有限非退化输入下得到单位正交基；朝上方向使用 up_hint 在成像平面的投影。
// 投影过小时改用**与视线最不对齐的世界轴**，避免单位化近零向量。
// 阈值与回退轴切换不承诺滚转连续性，详见 set_basis_from_lookat。
class camera {
  public:
    camera(double aspect_ratio, double vh, double focal = 1.0)
    : aspect{aspect_ratio}, viewport_height{vh},
      viewport_width{aspect_ratio * vh}, focal_length{focal},
      ray_scale_x{aspect_ratio * vh }, ray_scale_y{vh } { }
    // 直接构造保持默认正交基与原点中心。

     ray get_ray(double u, double v) const{
        const double sx = (u - 0.5) * ray_scale_x;   // 使用物理视口尺度，不再额外除以 focal
        const double sy = (v - 0.5) * ray_scale_y;
        // 相机局部正交基中的未归一化分量为 (sx, sy, -focal)（世界方向由 right/up/forward 合成），再归一化；
        // 默认基且 focal=1 时方向为 (sx,sy,-1)。
        // [历史锚点·勿删] 保留尺度计算、乘加顺序与归一化位置；数学等价的重排不保证浮点逐位相同。
        return ray(center, unit_vector(right * sx + up * sy + forward * focal_length));
    }                   // u,v 通常在 [0,1]；本接口只接收连续坐标，不决定像素中心或采样策略。

    static camera from_vfov(double aspect_ratio, double vfov_deg, double focal = 1.0){
        double theta = vfov_deg * pi / 180.0;      // vfov 由度转为弧度（三角函数按弧度）
        double h = std::tan(theta / 2.0);          // 半高对应的 tan
        double vh = 2.0 * h * focal;
        return camera(aspect_ratio, vh, focal);
    }

    // ── 可移动机位 ─────────────────────────────────────────────────────────
    // 正焦距下中心射线方向为 unit(lookat − lookfrom)。
    // `up` 是**参考上方向**（允许与视线不垂直，会被投影到成像平面内）。
    static camera from_lookat(double aspect_ratio, double vh,
                              const point3& lookfrom, const point3& lookat,
                              const vec3& up_hint, double focal = 1.0){
        camera cam(aspect_ratio, vh, focal);
        cam.set_basis_from_lookat(lookfrom, lookat, up_hint);
        return cam;
    }
    // 与 from_vfov 同公式：vh 含 focal ⇒ 这两个工厂的视场角不随 focal 改变。
    static camera from_lookat_vfov(double aspect_ratio, double vfov_deg,
                                   const point3& lookfrom, const point3& lookat,
                                   const vec3& up_hint, double focal = 1.0){
        const double theta = vfov_deg * pi / 180.0;
        return from_lookat(aspect_ratio, 2.0 * std::tan(theta / 2.0) * focal,
                           lookfrom, lookat, up_hint, focal);
    }

  private:
    // M_PI 非标准 C++，std::numbers::pi 要求 C++20；这里用字面常量兼容 C++17。
    // 工厂参数用度数；三角函数使用弧度。
    // FOV 前置由调用者保证，工厂不会钳位非法视角。
    static constexpr double pi = 3.14159265358979323846;

    // 由 lookfrom/lookat/up 定出正交基（**退化保护**）：
    // up 平行于 forward 或为零时，输入无法确定滚转；直接单位化叉积会除零。
    // ① 将 up 投影到成像平面，去掉前向分量。
    // ② 投影长度平方 < 1e-12 时回退；up_hint 未先归一化，这是绝对长度阈值，
    //    因而缩放同一 up_hint 也可能改变分支。
    // ③ 选 forward **绝对值最小的分量**对应世界轴，并列优先 x，再 y。
    //    该轴与视线最不对齐，投影长度有下界，避免单位化近零向量。
    // ④ 将回退轴投影并归一化，再构造 right 和 up。
    // **限制**：阈值和选轴切换处可能出现滚转跳变，不保证连续性。
    // 此回退只处理上方向投影退化，不检查零视线或一般 NaN/Inf。
    void set_basis_from_lookat(const point3& lookfrom, const point3& lookat, const vec3& up_hint){
        center   = lookfrom;
        forward  = unit_vector(lookat - lookfrom);

        vec3 up_ref = up_hint - dot(up_hint, forward) * forward;   // ① 投影到成像平面
        if (up_ref.length_squared() < 1e-12) {                  // ② 绝对投影长度阈值（也包含零 up）
            const double ax = std::fabs(forward.x());
            const double ay = std::fabs(forward.y());
            const double az = std::fabs(forward.z());
            up_ref = (ax <= ay && ax <= az) ? vec3(1.0, 0.0, 0.0)     // ③ 与视线最不对齐的轴
                   : ((ay <= az)            ? vec3(0.0, 1.0, 0.0)
                                            : vec3(0.0, 0.0, 1.0));
            up_ref = up_ref - dot(up_ref, forward) * forward;   // 回退轴再投影一次（更稳）
        }
        up_ref = unit_vector(up_ref);
        right = unit_vector(cross(forward, up_ref));
        up      = cross(right, forward);
    }

    double aspect, viewport_height, viewport_width, focal_length;
    double ray_scale_x, ray_scale_y;
    point3 center{0, 0, 0};
    // 默认正交基：右为 x，上为 y，前为 -z。
    vec3 right  {1, 0,  0};
    vec3 up     {0, 1,  0};
    vec3 forward{0, 0, -1};
};
