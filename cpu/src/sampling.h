// # 样本种子与像素抖动
// 每个样本按 (seed,i,j,s) 独立播种，避免线程调度和其它样本路径长度影响取数。
// 工程 pixel_color 内联同语义取数；sample_uv 是供测试使用的封装。
// **无可变共享状态**；并发调用时输出引用仍须独立，浮点分布不承诺跨标准库逐位一致。
#pragma once
#include <cstdint>
#include <random>

// ── mix3：把 (seed, i, j, s) 拌匀成一个 64 位种子 ────────────────────
// 前置：seed 任意 uint64_t；i、j 任意 int（本项目实际传像素坐标 0 <= i < img_w、0 <= j < img_h，
//       但本函数**不依赖**该范围）；s 由【调用方】保证是样本下标 0 … spp-1（本函数不做检查）。
// 后置：**纯函数** —— 同一组四元组在【任意时刻、任意线程、任意调用顺序】下返回**逐位相同**的值；
//       不读全局、不读静态、不读时间、不读线程 id。
// 说明：不同参数**期望**得到不同结果，但**不承诺无碰撞**（输入四元组映射到有限的 64 位种子空间）。
inline uint64_t mix3(uint64_t seed, int i, int j, int s) {
    uint64_t h = seed;
    h ^= uint64_t(i) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= uint64_t(j) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= uint64_t(s) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

// 取像素 (i,j) 的第 s 个样本的抖动偏移，不是归一化图像坐标。
// 前置：s 是样本下标而非累计抽数次数；u/v 为不同的可写对象；此处不检查下标。
// 新建局部引擎并以 mix3(base,i,j,s) 播种，**先取 u、再取 v，各调用分布一次**，结果在 [0,1)。
// 引擎在返回时销毁。工程热路径自己创建引擎，取抖动后交给 ray_color 继续用于散射。
// 工程 spp==1 不用此封装：像素中心不抽抖动数，但散射仍使用既有像素种子取数。
inline void sample_uv(uint64_t base, int i, int j, int s,
                      double& u, double& v) {
    std::mt19937_64 eng(mix3(base, i, j, s));
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    u = dist(eng);
    v = dist(eng);
}
