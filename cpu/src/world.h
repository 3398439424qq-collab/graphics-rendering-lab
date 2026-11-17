#pragma once
#include "hittable.h"     // 基类 hittable、hit_record（并经它带来 ray / interval / vec3）
#include "aabb.h"         // bounding_box() 的返回类型
#include "bvh_node.h"     // root_ 的类型 + bvh_node::build
#include <cstdlib>        // std::exit（硬失败路径）
#include <iostream>       // std::cerr 诊断输出
#include <memory>         // shared_ptr / unique_ptr
#include <utility>        // std::move（add 里移交句柄）
#include <vector>         // objects_

struct world : public hittable {
public:
    world() = default;

    // 接收共享所有权；空句柄报错 exit(3)。添加后旧 root 失效。
    // 即使 BVH 开关仍开，也须重建后才能对非空集合 hit。
    void add(std::shared_ptr<hittable> object) {
        if (!object) {
            std::cerr << "FATAL: world::add called with null object\n";
            std::exit(3);
        }
        objects_.emplace_back(std::move(object));
        root_.reset();
    }

    // 原地重排对象句柄并独占新树；空集合清空 root。
    // 调用者保证对象盒有效，且树使用期间几何与盒不变。
    void build_bvh() {
        if (objects_.empty()) {
            root_.reset();
        } else {
            root_ = bvh_node::build(objects_, 0, objects_.size());
        }
    }

    void set_use_bvh(bool enabled) { use_bvh_ = enabled; }

    bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
        if (objects_.empty()) { return false; }

        if (use_bvh_) {
            // 非空集合开启 BVH 后必须已有对应当前集合的树；否则报错 exit(3)。
            if (root_ == nullptr) {
                std::cerr << "FATAL: world::hit called in S2 "
                             "(BVH mode, non-empty objects, null root)\n";
                std::exit(3);
            }
            // 树已构建，转发开区间最近命中及 false 不改 rec 契约。
            return root_->hit(r, ray_t, rec);
        }

        // 暴力模式不使用 root；空集合在入口返回 false 且不改 rec。
        hit_record temp_rec;
        bool hit_any = false;
        double closest = ray_t.max;   // 当前求交窗口的参数上界

        for (const auto& obj : objects_) {
            // 上界使用当前最小 t，后续对象只可用更小参数覆盖命中。
            if (obj->hit(r, interval(ray_t.min, closest), temp_rec)) {
                hit_any = true;
                closest = temp_rec.t;
                rec = temp_rec;       // 只有命中才写 rec
            }
        }
        return hit_any;
    }

    aabb bounding_box() const override {
        aabb box;
        for (const auto& object : objects_) {
            box.surround_box(object->bounding_box());
        }
        return box;
    }

    static world with_bvh(world w) {        // 按值收：接住工厂返回的临时对象
    w.build_bvh();
    w.set_use_bvh(true);
    return w;                           // 移动返回集合及树的所有权
    }

    bool has_emissive() const noexcept override {
        for (const auto& obj : objects_) {
            if (obj && obj->has_emissive()) return true;
        }
        return false;
    }

private:
    std::vector<std::shared_ptr<hittable>> objects_;
    std::unique_ptr<bvh_node> root_;
    bool use_bvh_ = false;
};


