#pragma once
#include "hittable.h"
#include "material.h"
#include <memory>
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <utility>

struct quad : hittable
{
    public:
        // 表示 Q+alpha*u+beta*v，alpha/beta 在 [0,1]；u×v 决定正面。
        // 材质共享持有，调用者保证非空及几何/中间运算有限。
        // 零边或 sin²<=1e-20 报错 exit(3)，但不做通用非有限值检查。
        quad(const point3& Q, const vec3& u, const vec3& v, std::shared_ptr<const material> mat)
        : Q_(Q), u_(u), v_(v), mat_(std::move(mat))
        {
            n_ = cross(u_,v_);
            n_dot_n_ = dot(n_,n_);
            double uu = dot(u_, u_);
            double vv = dot(v_, v_);
            if (uu == 0.0 || vv == 0.0) {
                std::cerr << "FATAL: quad: zero-length edge vector\n";
                std::exit(3);
            }

            // 用 sin^2 判断是否平行
            double sin2 = n_dot_n_ / (uu * vv);
            if (sin2 <= 1e-20) {
                std::cerr << "FATAL: quad: u and v are parallel (or nearly)\n";
                std::exit(3);
            }

            unit_n_ = unit_vector(n_);
            D_ = dot(unit_n_,Q_);
        }

        // 射线 t 用开区间，板边界用闭区间；仅确认板内后写 rec，false 不改 rec。
        // 平行不设阈值：通常浮点语义下产生的 Inf/NaN t 被 surrounds 拒绝。
        // 调用者保证非平行求交的中间运算可表示，避免 alpha/beta 为 NaN 绕过边界比较。
        bool hit(const ray& r,const interval& ray_t,hit_record& rec)const override{
            double denom = dot(unit_n_, r.direction());
            double t = (D_ - dot(unit_n_, r.origin())) / denom;
            if(ray_t.surrounds(t)){
                const point3 p = r.at(t);
                const vec3 w = p - Q_;
                double alpha = dot(cross(w, v_), n_) / n_dot_n_;
                double beta  = dot(cross(u_, w), n_) / n_dot_n_;
                if (alpha < 0.0 || alpha > 1.0 || beta < 0.0 || beta > 1.0) return false;
                rec.t    = t;
                rec.p    = r.at(t);
                rec.mat  = mat_.get();
                rec.u    = alpha;
                rec.v    = beta;
                rec.set_face_normal(r.direction(), unit_n_);   // 写 rec.front_face 与 rec.normal
                return true;
            }
            return false;
        }

        aabb bounding_box() const override{
            point3 p0 = Q_;
            point3 p1 = Q_ + u_;
            point3 p2 = Q_ + v_;
            point3 p3 = Q_ + u_ + v_;

            double min_x = std::min({p0.x(), p1.x(), p2.x(), p3.x()});
            double max_x = std::max({p0.x(), p1.x(), p2.x(), p3.x()});
            double min_y = std::min({p0.y(), p1.y(), p2.y(), p3.y()});
            double max_y = std::max({p0.y(), p1.y(), p2.y(), p3.y()});
            double min_z = std::min({p0.z(), p1.z(), p2.z(), p3.z()});
            double max_z = std::max({p0.z(), p1.z(), p2.z(), p3.z()});
            // 四角点逐轴取极值，覆盖任意朝向的平行四边形。

            // 逐轴按坐标/跨度尺度加 padding，避免平面盒零厚度及边界舍入漏剪枝。
            auto pad_axis = [](double lo, double hi) -> double {
                double extent = hi - lo;
                double scale = std::max({1.0, std::abs(lo), std::abs(hi), extent});
                return 1e-9 * scale;
            };

            double pad_x = pad_axis(min_x, max_x);
            double pad_y = pad_axis(min_y, max_y);
            double pad_z = pad_axis(min_z, max_z);

            return aabb(
                point3(min_x - pad_x, min_y - pad_y, min_z - pad_z),
                point3(max_x + pad_x, max_y + pad_y, max_z + pad_z)
            );
        }

        bool has_emissive() const noexcept override {
            return mat_ && mat_->is_emissive();
        }

    private:
        point3 Q_;
        vec3 u_, v_;
        std::shared_ptr<const material> mat_;
        vec3 n_;
        vec3 unit_n_;
        double n_dot_n_ = 0.0;
        double D_ = 0.0;
};
