#pragma once
#include"vec3.h"
#include"ray_hand.h"     // 提供 ray 类型；仅声明 const ray& 参数本可用前置声明
#include"interval.h"
#include"aabb.h"


class material; 
// 命中点、射线参数及朝入射侧的单位法线；front_face 区分几何正反面。
// u/v 是几何对象定义的纹理坐标：球面映射或平行四边形局部坐标。
struct hit_record{
    point3 p;
    vec3 normal;
    double t = 0.0;
    double u = 0.0;
    double v = 0.0;
    bool front_face = false;
    // 借用，不持有所有权；使用记录时材质须仍存活，通常由几何对象共享持有。
    const material* mat = nullptr;
    
    // 调用者保证 outward_normal 单位长、dir 有限非零；此处只翻转，不归一化。
    // 切触 dot==0 归为背面，normal 与 dir 点积不大于零。
    void set_face_normal(const vec3& dir, const vec3& outward_normal){
        front_face = dot(dir,outward_normal)<0;
        normal = front_face ? outward_normal : -outward_normal; 
    }
};

class hittable{
    public:
        virtual ~hittable() = default;
        // 多态基类禁止"按值拷贝"：基类拷贝 = 切片风险（派生类那部分被切掉），编译器不报
        hittable(const hittable&) = delete;
        hittable& operator=(const hittable&) = delete;
        // 显式保留移动：声明拷贝操作及析构会抑制隐式移动生成。
        // 派生集合可转移所有权并按值返回；移动不等于多态按值复制。
        hittable() = default;
        hittable(hittable&&) = default;
        hittable& operator=(hittable&&) = default;
        // true：记录开区间 (min,max) 内最小 t 的几何命中及单位 normal；false：不改 rec。
        // 调用者保证射线有限且方向非零、端点无 NaN；t 是参数而非天然距离。
        // 等 t 对象归属不作保证；记录中的 mat 借用材质，使用时须保证其存活。
        virtual bool hit(const ray& r,const interval& ray_t,hit_record& rec) const = 0;
        // 按值返回覆盖全部几何的盒；空集合可返回空盒。
        // 用于 BVH 的对象须提供非空、有限、中心有限的有效盒。
        virtual aabb bounding_box() const = 0;
        virtual bool has_emissive() const noexcept { return false; }
};
