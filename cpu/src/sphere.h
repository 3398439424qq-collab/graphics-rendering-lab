#pragma once
#include"hittable.h"
#include "material.h"
#include <memory>
#include <cmath>
#include <utility>   // std::move

class sphere : public hittable
{
    point3 center;
    double radius;
    std::shared_ptr<const material> mat;  
    public:
    sphere() = delete;
    // 3 参构造：材质与几何分离（玻璃球/金属球只是材质不同）
    //   shared_ptr<const material> 共享所有权；调用者保证材质非空、中心有限、半径有限且 >0（不检查）。
    sphere(const point3& c, double r, std::shared_ptr<const material> m)
        : center{c}, radius{r}, mat{std::move(m)} {}

    // 先试较小根再试较大根，返回开区间内最近命中；失败不改 rec。
    // 保留方向平方项，方向不必单位长；调用者保证中间运算有限且非退化。
    // UV 使用外向法线：u 接缝位于 -x 方位，v 从南极 0 到北极 1，两极的 u 不唯一。
    // 当前 acos 不钳位；浮点舍入若使参数越出 [-1,1]，可能产生 NaN。
    bool hit(const ray& r,const interval& ray_t,hit_record& rec) const override{
        double a = dot(r.direction(),r.direction());
        vec3 oc = r.origin()-center;
        double half_b = dot(oc, r.direction());
        double c=dot(oc,oc)-radius*radius;
        double disc = half_b*half_b - a*c;
        if(disc<0){return false;}
    
        double disc_sqrt = std::sqrt(disc);
        double t1 = (-half_b - disc_sqrt) / a;
        double t2 = (-half_b + disc_sqrt) / a;
        double PI = 3.14159265358979323846;

        if(ray_t.surrounds(t1)){ 
            rec.t = t1;
            rec.p = r.at(rec.t);
            rec.mat = mat.get();
            vec3 outward = (rec.p - center) / radius;
            rec.set_face_normal(r.direction(), outward);
            rec.u = 1 - (std::atan2(outward.z(),outward.x())+PI)/(2*PI);
            rec.v = 1 - std::acos(-outward.y())/PI;
            return true;
        }
        if(ray_t.surrounds(t2)){ 
            rec.t = t2;
            rec.p = r.at(rec.t);
            rec.mat = mat.get();
            vec3 outward = (rec.p - center) / radius;
            rec.set_face_normal(r.direction(), outward);
            rec.u = 1 - (std::atan2(outward.z(),outward.x())+PI)/(2*PI);
            rec.v = 1 - std::acos(-outward.y())/PI;
            return true;
        }
        return false;
    }

    aabb bounding_box() const override {
        vec3 lo = vec3(center.x() - radius, center.y() - radius, center.z() - radius);
        vec3 hi = vec3(center.x() + radius, center.y() + radius, center.z() + radius);
        return aabb(lo, hi);
    }

    bool has_emissive() const noexcept override {
        return mat && mat->is_emissive();
    }
};
