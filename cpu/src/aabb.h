#pragma once
#include "vec3.h"
#include"interval.h"
#include"ray_hand.h"
#include <limits>
#include <algorithm>

struct aabb{
    private:
        vec3 lo_;
        vec3 hi_;
        // 盒按值保存端点；默认空盒作为包围盒并集的恒等元。
        // 显式端点构造不排序、不检查输入；调用者保证端点无 NaN。
        // 非空几何盒要求各轴 lo <= hi；供 BVH 使用时还须有限且中心有限。
    public:
        aabb()
        : lo_(std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity()),
          hi_(-std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity()) {} // 默认构造空盒：lo_ 各分量为正无穷，hi_ 各分量为负无穷。
        aabb(const vec3& lo, const vec3& hi):lo_{lo},hi_{hi}{}

        // ── 建树访问器：调用者保证 axis 为 0、1 或 2 ──
        //    契约：对"空盒"（lo=+inf, hi=-inf）lo/hi 分别返回 ±inf；
        //          ⚠️ 空盒上调用 center() 会得到 (+inf + -inf)/2 = NaN ⇒
        //          比较器**不允许**对空盒取 center（nth_element 的比较器必须满足严格弱序，
        //          NaN 会让它不满足 ⇒ 行为未定义）。建树里只对"非空盒对象"取 center。
        double lo(int axis) const     { return lo_[axis]; }
        double hi(int axis) const     { return hi_[axis]; }
        double center(int axis) const { return 0.5 * (lo_[axis] + hi_[axis]); }

        // 取最长轴；调用者保证非空有限盒，跨度运算可表示，并列优先 x、再 y。
        //   使用 hi-lo 的正跨度；若改成 lo-hi，取最大值反而选到最短轴。
        //   选轴影响空间划分与剪枝成本，但不应改变最近命中的几何契约。
        int longest_axis() const {
            const double dx = hi_[0] - lo_[0];
            const double dy = hi_[1] - lo_[1];
            const double dz = hi_[2] - lo_[2];
            if (dx >= dy && dx >= dz) return 0;
            if (dy >= dz)             return 1;
            return 2;
        }
        bool empty() const {
            return lo_[0] > hi_[0] || lo_[1] > hi_[1] || lo_[2] > hi_[2];} // 只要任一轴的 lo 大于对应的 hi，就是空盒。

        // 保守闭窗口盒测试：true 仅表示可能触及盒子，含端点/切触，不是几何命中。
        // false 可安全剪枝，不写 hit_record；调用者保证射线/盒端点与区间无 NaN。
        // 方向分量为零时直接测起点所在 slab，避免 0*Inf 引入 NaN。
        bool hit(const ray& r, const interval& ray_t) const{
            if (empty()) return false;
            double t_max = ray_t.max;
            double t_min = ray_t.min;
            if (t_min > t_max) return false;
            const vec3& o = r.origin();
            const vec3& d = r.direction();

            for(int axis=0;axis<3;axis++){
                const double oi = o[axis];
                const double di = d[axis];
                const double lo = lo_[axis];
                const double hi = hi_[axis];

                if(di == 0.0){if (oi < lo || oi > hi) return false;}
                else{
                    double t0 = (lo-oi)/di;
                    double t1 = (hi-oi)/di;
                    if (t0 > t1) {
                    double tmp = t0;
                    t0 = t1;
                    t1 = tmp;
                    }
                    t_min = std::max(t_min, t0);
                    t_max = std::min(t_max, t1);

                    if (t_min > t_max) return false;
                }
            }
            return true;
        }
        aabb& surround_box(const aabb& b){// 将 b 并入 *this 并返回 *this；空盒是并集恒等元（*this 为空则结果为 b，b 为空则 *this 不变）。
            lo_[0] = std::min(lo_[0],b.lo_.x());
            lo_[1] = std::min(lo_[1],b.lo_.y());
            lo_[2] = std::min(lo_[2],b.lo_.z());

            hi_[0] = std::max(hi_[0],b.hi_.x());
            hi_[1] = std::max(hi_[1],b.hi_.y());
            hi_[2] = std::max(hi_[2],b.hi_.z());
            return *this;
        }
};
