#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// bvh_node.h —— BVH 节点（叶 = 一小组几何对象；内部 = 两棵子树）
// [契约指针] 正确性判据（仓库根相对路径）：练习/工程专项/CPU-07_BVH与场景管理/工程集成/工程_bvh_node_chk.cpp 文件头 C1~C6。
// 覆盖暴力对照、最近命中、未命中记录不变、空集合及根盒并集；不验证剪枝性能。
//
// 所有权模型：
//   · 根由 unique_ptr 独占；nodes_ 连续存储全部节点，孩子用下标寻址。
//   · objects 是全树 shared_ptr 几何句柄池，与暴力路径共享几何对象。
//   · 默认构造私有，仅 build() 创建空壳；空区间返回 nullptr。
// 不变量：根固定下标0；叶 obj_count>0，区间定位池内对象；内部 obj_count==0。
// 叶盒/内部盒分别是对象/孩子盒并集；扩容期间只保存下标，遍历只读。
//
// 本文件是 header-only 工程的一部分，可被多个翻译单元 include
//    ⇒ 所有定义都写在类体里（隐式 inline）；若改写成类外定义，必须显式加 `inline`，
//    否则链接期报 `multiple definition`。
//
// 交付实现：节点数组＋全树对象池；布局对照宏/旧指针实现已移除。
// CLI 的 --bvh 0/1 保留，用于暴力/BVH正确性对照，不是布局版本切换。
// ─────────────────────────────────────────────────────────────────────────────
#include "hittable.h"
#include "aabb.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

struct FlatNode {
    aabb box;                                        // 原 box_
    int  left = -1, right = -1;                      // 子节点下标（叶不写）
    int obj_begin = 0;
    int obj_count = 0;
};

class bvh_node : public hittable {
public:
    // ── 递归建树（扁平数组版，静态工厂）────────────────────────────────────
    // 空区间 ⇒ nullptr；否则创建空壳节点，再由 build_flat 填充 nodes_。
    static std::unique_ptr<bvh_node> build(std::vector<std::shared_ptr<hittable>>& objs,
                                           std::size_t start, std::size_t end) {
        if (start >= end) return nullptr;

        std::unique_ptr<bvh_node> root(new bvh_node());
        root->nodes_.reserve(2 * objs.size());   // 叶阈值 2 ⇒ 节点数 ≤ 2n−1；仅优化，正确性不依赖
        const int root_idx = root->build_flat(objs, start, end);
        assert(root_idx == 0);
        return root;
    }

    // ── 求交 ────────────────────────────────────────────────────────────────
    bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
        if (nodes_.empty()) return false;
        return hit_flat(0, r, ray_t, rec);
    }

    aabb bounding_box() const override {
        return nodes_.empty() ? aabb() : nodes_[0].box;
    }

    bool has_emissive() const noexcept override {
    for (const auto& x : objects) {
        if (x->has_emissive()) {
            return true;
        }
    }
    return false;
    }

private:
    bvh_node() = default;   // 私有：仅供 build() 创建空壳，随后填充 nodes_

    // ── 扁平数组建树：父先子后，先占位，再递归左右，最后回填 ─────────────
    int build_flat(std::vector<std::shared_ptr<hittable>>& objs,
                   std::size_t start, std::size_t end) {
        assert(start < end);

        // 先占 my = nodes_.size()，用 push_back 占位。
        // 全程只保留下标，不保存 FlatNode&，避免 push_back 重分配后引用失效。
        const int my = static_cast<int>(nodes_.size());
        nodes_.push_back(FlatNode{});

        // 叶阈值仍是 2。
        if (end - start <= 2) {
            const int begin = static_cast<int>(objects.size());
            const int count = end - start;
            assert(count > 0);
            for (std::size_t i = static_cast<std::size_t>(start);
                 i < static_cast<std::size_t>(end); ++i) {
                objects.push_back(objs[i]);
            }
            aabb leaf_box = objs[static_cast<std::size_t>(start)]->bounding_box();
            for (std::size_t i = static_cast<std::size_t>(start) + 1;
                 i < static_cast<std::size_t>(end); ++i) {
                leaf_box.surround_box(objs[i]->bounding_box());
            }
            nodes_[my].box = leaf_box;
            nodes_[my].obj_begin = begin;
            nodes_[my].obj_count = count;
            return my;
        }

        // 本层并集盒 → 最长轴
        aabb seg_box;
        for (std::size_t i = start; i < end; ++i) {
            seg_box.surround_box(objs[i]->bounding_box());
        }
        const int axis = seg_box.longest_axis();

        // 中位数分区：同一个 nth_element 轴与中位
        const std::size_t mid = start + (end - start) / 2;
        const auto cmp = [axis](const std::shared_ptr<hittable>& a,
                                const std::shared_ptr<hittable>& b) {
            return a->bounding_box().center(axis) < b->bounding_box().center(axis);
        };
        std::nth_element(objs.begin() + start, objs.begin() + mid, objs.begin() + end, cmp);

        // 先左后右递归
        const int L = build_flat(objs, start, mid);
        const int R = build_flat(objs, mid, end);

        // 构建期断言：子下标有效
        assert(L >= 0 && L < static_cast<int>(nodes_.size()));
        assert(R >= 0 && R < static_cast<int>(nodes_.size()));

        // 最后回填 nodes_[my]
        aabb inner_box;
        inner_box.surround_box(nodes_[L].box).surround_box(nodes_[R].box);

        nodes_[my].box = inner_box;
        nodes_[my].left = L;
        nodes_[my].right = R;

        // 构建期断言：left/right 有效，内部 obj_count 为 0
        assert(nodes_[my].left >= 0 && nodes_[my].left < static_cast<int>(nodes_.size()));
        assert(nodes_[my].right >= 0 && nodes_[my].right < static_cast<int>(nodes_.size()));
        assert(nodes_[my].obj_count == 0);

        return my;
    }

    // ── 扁平数组求交：按下标递归 ───────────────────────────────────────────
    bool hit_flat(int idx, const ray& r, const interval& ray_t, hit_record& rec) const {
    const FlatNode& node = nodes_[idx];
    if (!node.box.hit(r, ray_t)) return false;

    if (node.obj_count > 0) {
        bool hit_any = false;
        interval window = ray_t;
        for (int i = 0; i < node.obj_count; i++) {
            const auto& obj = objects[node.obj_begin + i];
            if (obj->hit(r, window, rec)) {
                hit_any = true;
                window.max = rec.t;
            }
        }
        return hit_any;
    }

    interval window = ray_t;
    bool hit_left = false;
    if (node.left >= 0) {
        hit_left = hit_flat(node.left, r, window, rec);
    }
    if (hit_left) window.max = rec.t;

    bool hit_right = false;
    if (node.right >= 0) {
        hit_right = hit_flat(node.right, r, window, rec);
    }
    return hit_left || hit_right;
}

    std::vector<FlatNode> nodes_;
    std::vector<std::shared_ptr<hittable>> objects;
};