// # CPU 路径追踪渲染器
// 入口流程：解析参数 → 装配场景与相机 → 按 tile 并行采样 → 写出 PPM。
// `default` 输出 12 张对照图；`cornell` / `checker` 各输出一张。
// 颜色在线性空间累加、平均，显示变换与量化只在 write_color 中执行。
// 输出路径相对于运行时工作目录；调用者应在独立输出目录运行，避免覆盖已有图像。
// 当前仅 `cornell` 分支写出计时数据；计时字段的边界见 timing::RenderSplit。


#include "sphere.h"
#include "material.h"
#include "vec3.h"
#include "ray_hand.h"
#include "camera.h"
#include "world.h"
#include "quad.h"
#include "color.h"
#include "sampling.h"
#include "fatal.h"
#include "timing.h"

#include <fstream>
#include <iostream>
#include<cmath>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <cstring>
#include <utility>
#include <cstdint>      // uint64_t
#include <cstdlib>
#include <cerrno>
#include <mutex>
#include <thread>       // 工作线程
#include <vector>       // 像素缓冲与线程池
#include <algorithm>    // std::min 钳位块上界

// ## 渲染配置
// 尺寸由 CLI 设置并在 main 中检查，每维允许 1~8192。
// **解析与检查完成后不再修改**；所有工作线程只读这些配置。
int    img_w  = 400;
int    img_h  = 225;
constexpr int MAX_DIM = 8192;      // 本程序的尺寸与 spp 上限
constexpr double MIN_IOR = 0.1;    // 演示用 ior 限制：非物理通用域
constexpr double MAX_IOR = 10.0;   // 上限使文件名缩放 int(ior*100) 落在 10~1000


// 工作线程数：默认 1，使默认资源使用不依赖机器的硬件并发提示。
// 与尺寸一样，CLI 解析期写入，渲染期间只读；并发读取不构成数据竞争。
// 图像采样流与线程数无关，可复现性由 pixel_color 的播种方式保证。
int    g_threads = 1;

// tile 边长（每块 = TILE×TILE 像素）。32 ⇒ 400×225 得 13×8 = 104 块。
//   取舍：块越少 ⇒ 领块加锁次数越少，但负载均衡粒度越粗；块越多则反之。
constexpr int TILE = 32;

// 未命中场景时，按非零射线方向生成蓝白渐变背景。
color background(const ray& r) {
    vec3 unit_dir = unit_vector(r.direction());
    double t = 0.5 * (unit_dir.y() + 1.0);
    return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
}

// 检查线性颜色的三个分量；发现 NaN/Inf 时报告位置并以退出码 3 终止。
// 注意：std::exit 不展开调用栈，不会执行栈上对象的析构。
static void require_finite(const color& c, const char* where) {
    if (!(std::isfinite(c.x()) && std::isfinite(c.y()) && std::isfinite(c.z()))) {
        std::cerr << "FATAL: 非有限数值出现在 " << where
                  << " —— 值为 (" << c.x() << ", " << c.y() << ", " << c.z() << ")\n";
        std::exit(3);
    }
}

// 沿一条样本光路累加发光与背景，返回线性颜色（尚未做显示变换）。
// 前置：max_depth >= 0，射线方向有效，命中记录带有有效材质。
// eng 属于当前样本，每次尝试散射依次抽取两个随机数。
// 深度耗尽时直接截断；命中后先累加发光，再决定是否继续散射。
color ray_color(const ray& r_in, const hittable& world, int max_depth,
                std::mt19937_64& eng) {
    std::uniform_real_distribution<double> U(0.0, 1.0);

    color accumulated(0.0, 0.0, 0.0);
    color throughput(1.0, 1.0, 1.0);
    ray   r        = r_in;
    int   bounces  = 0;

    for (;;) {
        // 深度预算耗尽：保留已累加结果，不再求交或补背景。
        if (bounces == max_depth) return accumulated;

        // 未命中：将当前吞吐量乘背景并累加，结束光路。
        hit_record rec;
        if (!world.hit(r,
                interval(0.001, std::numeric_limits<double>::infinity()),
                rec)) {
            const color c = accumulated + throughput * background(r);
            require_finite(c, "ray_color/未命中累加");
            return c;
        }

        // 渲染所需的命中记录必须带材质；几何求交接口本身不强制这一点。
        if (rec.mat == nullptr) {
            std::cerr << "FATAL: 命中点没有材质（sphere 构造未传 material？）\n";
            std::exit(3);
        }

         // 累加 emitted（必须在 scatter 之前）
        const color emit = rec.mat->emitted(
            rec.u, rec.v, rec.p, rec.front_face
        );
        accumulated += throughput * emit;
        require_finite(accumulated, "ray_color/累加 emitted");

        // 无后续散射时返回已累加的颜色，包括本次命中的发光。
        color attenuation;
        ray   scattered;
        const double u1 = U(eng);
        const double u2 = U(eng);
        if (!rec.mat->scatter(r, rec, attenuation, scattered, u1, u2)) {
            return accumulated;
        }
        require_finite(attenuation, "scatter/attenuation");

        // 一次散射完成：先乘权重，再换光线
        throughput = throughput * attenuation;
        require_finite(throughput, "ray_color/throughput");
        r          = scattered;
        ++bounces;
    }
}

// 像素中心分支的种子混合：固定输入返回固定值，不承诺不同坐标无碰撞。
uint64_t mix(uint64_t seed, int x, int y) {
    uint64_t h = seed;
    h ^= uint64_t(x) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= uint64_t(y) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

// 按十进制解析到 long：允许 strtol 支持的前导空白与正负号，拒绝尾随字符和范围错误。
// 失败返回 false 且不改 out；选项的有效范围由调用方另行检查。
bool parse_long(const char* s, long& out) {
    if (s == nullptr || *s == '\0') return false;
    errno = 0;
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return false;   // 空串 / 含多余字符 / 溢出
    out = v;
    return true;
}

// 使用 strtod 解析整个字符串并拒绝 NaN/Inf 与范围错误；失败不改 out。
// 接受的数字格式遵循 strtod（不局限于普通十进制小数）。
bool parse_double(const char* s, double& out) {
    if (s == nullptr || *s == '\0') return false;
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0') return false;  // 空串/多余字符/溢出
    if (!std::isfinite(v)) return false;                        // ← 拦 nan / inf
    out = v;
    return true;
}

// 同一场景内并排展示漫反射与金属，便于比较材质外观。
//   左 = lambertian（漫反射）｜右 = metal（镜面反射，fuzz=0 为完美镜面）
// 输出名 pair_diffuse_metal*.ppm，几何布局不随 fuzz 改变。
world pair_scene(double fuzz_value) {
    world scene;
    const color gray(0.6, 0.6, 0.6);
    scene.add(std::make_shared<sphere>(point3(-1.15, 0.0, -3.2), 1.0,
              std::make_shared<lambertian>(gray)));                     // 左：漫反射
    scene.add(std::make_shared<sphere>(point3( 1.15, 0.0, -3.2), 1.0,
              std::make_shared<metal>(gray, fuzz_value)));              // 右：金属
    scene.add(std::make_shared<sphere>(point3(0.0, -4.8, -3.2), 4.0,
              std::make_shared<lambertian>(color(0.5, 0.5, 0.5))));    // 地面（顶部 y=-0.8，提供"反射内容"）
    return scene;
}

world base_scene() {                                    // 基准场景
    world scene;
    scene.add(std::make_shared<sphere>(point3(0, 0, -3), 1.0,
              std::make_shared<lambertian>(color(0.6, 0.6, 0.6))));
    return scene;
}

world multi_scene() {                                   // 多球位置对照
    world scene;
    auto gray = []{ return std::make_shared<lambertian>(color(0.6, 0.6, 0.6)); };
    scene.add(std::make_shared<sphere>(point3(0, 0, -3),          1.0,  gray()));   // A 基准
    scene.add(std::make_shared<sphere>(point3(-1.25, 0.15, -2.2), 0.45, gray()));  // B 近小左
    scene.add(std::make_shared<sphere>(point3(1.6, -0.2, -4.5),   0.9,  gray()));   // C 远大右
    scene.add(std::make_shared<sphere>(point3(0, 0, 3),           1.0,  gray()));   // D 相机后
    scene.add(std::make_shared<sphere>(point3(3.4, 0.9, -4.0),    0.8,  gray()));   // E 右出画
    return scene;
}

// 材质对照：一颗漫反射球与四颗不同 fuzz 的金属球。
//   · 几何与 multi_scene() 完全相同，只换材质 ⇒ 可逐像素对照"材质差异"
//   · fuzz 由调用方参数派生：0（完美镜面）／--fuzz／更粗一档／mid
world metal_scene(double fuzz_value) {
    world scene;
    auto gray = []{ return color(0.6, 0.6, 0.6); };
    const double f_coarse = (fuzz_value < 0.5) ? (fuzz_value * 2.0) : 1.0;
    const double f_mid    = (fuzz_value < 0.5) ? (fuzz_value * 0.5) : 0.5;
    scene.add(std::make_shared<sphere>(point3(0, 0, -3),          1.0,
              std::make_shared<lambertian>(gray())));                  // A 漫反射（对照基准）
    scene.add(std::make_shared<sphere>(point3(-1.25, 0.15, -2.2), 0.45,
              std::make_shared<metal>(gray(), 0.0)));                  // B 完美镜面 fuzz=0
    scene.add(std::make_shared<sphere>(point3(1.6, -0.2, -4.5),   0.9,
              std::make_shared<metal>(gray(), fuzz_value)));           // C 金属 fuzz = --fuzz
    scene.add(std::make_shared<sphere>(point3(0, 0, 3),           1.0,
              std::make_shared<metal>(gray(), f_coarse)));             // D 相机后（更粗一档）
    scene.add(std::make_shared<sphere>(point3(3.4, 0.9, -4.0),    0.8,
              std::make_shared<metal>(gray(), f_mid)));                // E 右出画（更细一档）
    return scene;
}

// 介质对照场景：前方玻璃球、后方小红球、右侧不透明灰球及地面。
// 小红球作为折射观察目标，留出地面与天空使球内的反射／折射内容容易区分。
// 成像位置与大小随相机和 ior 改变；灰球提供不透明材质的对照。
world glass_scene(double ior) {
    world scene;
    scene.add(std::make_shared<sphere>(point3(-0.85, 0.05, -3.0), 0.85,
              std::make_shared<dielectric>(ior)));                      // 前：玻璃球
    scene.add(std::make_shared<sphere>(point3(-0.30, -0.42, -4.60), 0.38,
              std::make_shared<lambertian>(color(0.75, 0.30, 0.25))));  // 后：小红球，作为折射目标
    scene.add(std::make_shared<sphere>(point3(1.55, -0.10, -4.60), 0.70,
              std::make_shared<lambertian>(color(0.55, 0.55, 0.60))));  // 右后：灰球（不透视的对照）
    scene.add(std::make_shared<sphere>(point3(0.0, -4.8, -3.2), 4.0,
              std::make_shared<lambertian>(color(0.5, 0.5, 0.5))));     // 地面（顶部 y=-0.8）
    return scene;
}

// ## 场景装配
// 创建平行四边形并交给 scene 共享持有；材质必须有效，朝向由 cross(u,v) 决定。
void add_quad(world& scene, const point3& Q, const vec3& u, const vec3& v,
              std::shared_ptr<const material> mat) {
    scene.add(std::make_shared<quad>(Q, u, v, std::move(mat)));
}

// 创建 axis=fixed 的轴对齐矩形，dir=+1/-1 指定法线沿该轴的正／负方向。
// 区间映射：x → (y,z)，y → (x,z)，z → (x,y)；两轴各用自己的 min/max。
// 调用者提供有限坐标和有效材质；非法 axis/dir、非递增区间或朝向检查失败会退出（码 3）。
void add_axis_aligned_quad(world& scene, char axis, int dir,
                           double min1, double max1,
                           double min2, double max2,
                           double fixed,
                           std::shared_ptr<const material> mat) {
    if (dir != 1 && dir != -1) {
        std::cerr << "FATAL: add_axis_aligned_quad: dir 必须是 +1 或 -1\n";
        std::exit(3);
    }
    if (!(max1 > min1) || !(max2 > min2)) {
        std::cerr << "FATAL: add_axis_aligned_quad: 两个区间都必须 max > min\n";
        std::exit(3);
    }

    const double s1 = max1 - min1, s2 = max2 - min2;
    vec3   u(0.0, 0.0, 0.0), v(0.0, 0.0, 0.0), axisVec(0.0, 0.0, 0.0);
    point3 Q(0.0, 0.0, 0.0);

    if (axis == 'y') {                       // 平面 y=fixed；第一轴 x、第二轴 z
        Q       = point3(min1, fixed, min2);
        u       = vec3(s1, 0.0, 0.0);
        v       = vec3(0.0, 0.0, s2);
        axisVec = vec3(0.0, 1.0, 0.0);
    } else if (axis == 'x') {                // 平面 x=fixed；第一轴 y、第二轴 z
        Q       = point3(fixed, min1, min2);
        u       = vec3(0.0, s1, 0.0);
        v       = vec3(0.0, 0.0, s2);
        axisVec = vec3(1.0, 0.0, 0.0);
    } else if (axis == 'z') {                // 平面 z=fixed；第一轴 x、第二轴 y
        Q       = point3(min1, min2, fixed);
        u       = vec3(s1, 0.0, 0.0);
        v       = vec3(0.0, s2, 0.0);
        axisVec = vec3(0.0, 0.0, 1.0);
    } else {
        std::cerr << "FATAL: add_axis_aligned_quad: axis 必须是 'x'/'y'/'z'\n";
        std::exit(3);
    }

    // 三种轴的初始边向量不具有相同朝向；点积为负时交换两边，使法线匹配 dir。
    if (dot(cross(u, v), axisVec * double(dir)) < 0.0) std::swap(u, v);
    // 检查生成法线与请求方向同向，避免单面光源朝向被静默写反。
    const double dot_want = dot(unit_vector(cross(u, v)), axisVec * double(dir));
    if (!(dot_want > 0.0)) {
        std::cerr << "FATAL: add_axis_aligned_quad: 法线与 dir*axis 不同向（"
                  << dot_want << "）\n";
        std::exit(3);
    }
    add_quad(scene, Q, u, v, std::move(mat));
}

// 简化 Cornell 房间：五个灰色墙面和一块单面光源，无内部物体。
// light_intensity 是光源的线性发光颜色分量；CLI 通过 --light 设置，默认 1.0。
world cornell_scene(double light_intensity = 1.0){
    world scene;

    auto grey      = std::make_shared<lambertian>(color(0.72, 0.72, 0.72));
    auto light_mat = std::make_shared<diffuse_light>(
        color(light_intensity, light_intensity, light_intensity));

    // 房间范围：x∈[-2,2]、y∈[0,5]、z∈[-2,2]，+z 一面开口（相机侧）。
    // 每个面的 dir = **法线方向**：房间内侧看得到的那一面 ⇒ 从内侧射入时 front_face==true。
    //   地板 y=0    ⇒ dir=+1（朝 +y）      天花板 y=5  ⇒ dir=-1（朝 -y）
    //   左墙 x=-2   ⇒ dir=+1（朝 +x）      右墙   x=+2 ⇒ dir=-1（朝 -x）
    //   后墙 z=-2   ⇒ dir=+1（朝 +z）
    // ⚠️ 区间口径：每面要写**两个不同轴的区间**（四墙是 y:0~5 × z:−2~2，两轴跨度不同）
    //    axis='y' ⇒ (x区间, z区间)   axis='x' ⇒ (y区间, z区间)   axis='z' ⇒ (x区间, y区间)
    add_axis_aligned_quad(scene, 'y', +1, -2.0, 2.0, -2.0, 2.0, 0.0,  grey);   // 地板 (x,z)
    add_axis_aligned_quad(scene, 'y', -1, -2.0, 2.0, -2.0, 2.0, 5.0,  grey);   // 天花板 (x,z)
    add_axis_aligned_quad(scene, 'x', +1,  0.0, 5.0, -2.0, 2.0, -2.0, grey);   // 左墙 (y,z)
    add_axis_aligned_quad(scene, 'x', -1,  0.0, 5.0, -2.0, 2.0,  2.0, grey);   // 右墙 (y,z)
    add_axis_aligned_quad(scene, 'z', +1, -2.0, 2.0,  0.0, 5.0, -2.0, grey);   // 后墙 (x,y)

    // 天花板光源板：压在 y=4.999（避免与天花板 y=5 共面），朝房间内 ⇒ dir=-1（朝 -y）。
    // 单面发光由「dir ⇒ cross(u,v) 的符号」保证；若顺序写反，画面只是整块不发光、编译与运行都不报错
    // helper 检查生成法线与请求的 dir 一致；调用方仍须选对朝向。
    add_axis_aligned_quad(scene, 'y', -1, -0.8, 0.8, -0.6, 0.6, 4.999, light_mat);  // 光源板 (x,z)

    return scene;
}

// ## 像素采样
// 契约（仓库根相对路径）：练习/工程专项/CPU-11_随机数与调度/CPU-11_判据.md §1.3 ①~⑥、§六 Q2、§七。
// 返回像素 (i,j) 的线性平均颜色；调用者保证坐标在图像内、depth >= 0。
// **spp==1**：走像素中心，以 mix(base_seed,i,j) 播种；不抽抖动数，但光路散射仍抽数。
// **spp>1**：每样本以 mix3(base_seed,i,j,s) 播种，先抽 u、再抽 v，再将同一引擎交给 ray_color。
// 种子只由 (base_seed,i,j[,s]) 决定，不依赖时间、线程 ID、领取顺序或累计抽数次数。
// 同一数值实现下，每像素唯一写者、样本按 s 递增累加、取数顺序固定 ⇒ 结果不随线程数改变。
// **禁止**为优化而让不同像素/样本共用持续取数的引擎；加锁也不能消除调度或路径长度依赖。
// 同一个样本的抖动与散射须继续使用同一台引擎，不要为 ray_color 另播种。
// [历史锚点·勿删] spp==1 必须保留 mix(base_seed,i,j) 与像素中心，不抽抖动数、不改散射取数顺序；
// 换成 mix3、提前抽抖动数等都会改变随机流，破坏 5 张锚点 md5 不变的约束。
// **分层退出码**：CLI 对非法 spp 先返回 2，故 --spp 0 不会到达本函数。
// 本函数的 spp<1 检查是内部/未来绕过 CLI 调用的第二道闸，触发 FATAL 时退出码为 3。
static color pixel_color(const hittable& world, const camera& cam,
                         int i, int j, int spp, int depth, uint64_t base_seed) {
    if (spp < 1) FATAL("pixel_color: spp must be >= 1 (got " << spp << ")");
    if (spp == 1) {
        // 不抽像素抖动数，保留旧播种方式供后续散射使用。
        std::mt19937_64 eng(mix(base_seed, i, j));
        const double u = (double(i) + 0.5) / double(img_w);
        const double v = (double(j) + 0.5) / double(img_h);
        return ray_color(cam.get_ray(u, v), world, depth, eng);
    }

    std::uniform_real_distribution<double> U(0.0, 1.0);
    color sum(0, 0, 0);

    for (int s = 0; s < spp; ++s) {
        std::mt19937_64 eng(mix3(base_seed, i, j, s));  // 每样本一台引擎

        const double u01 = U(eng);   // 先抽 u
        const double v01 = U(eng);   // 再抽 v，各恰好一次

        const double u = (double(i) + u01) / double(img_w);
        const double v = (double(j) + v01) / double(img_h);

        sum += ray_color(cam.get_ray(u, v), world, depth, eng);
    }
    return sum / double(spp);
}

// 返回值：true = 打开成功且全部内容完整写出并成功关闭；false = 打开、写出/flush 或关闭任一失败。
// 失败时在本函数内以 stderr 输出带文件名的诊断（调用方不重复打印同一错误）。
bool render(const hittable& world, const camera& cam,
    int spp, int depth, uint64_t base_seed,
    const std::string& filename,timing::RenderSplit* split = nullptr) {      
    timing::Stopwatch sw;            // 秒表只在本函数内使用（局部，无共享状态）
    if (split) *split = {};          // 复位；打开图像失败时保持全零
    // 场景自检：只提示第一次，走 stderr，不污染 stdout
    static std::once_flag emissive_warn_flag;
    if (!world.has_emissive()) {
    std::call_once(emissive_warn_flag, [] {
        std::cerr << "warning: scene has no emissive material; "
                     "further identical warnings suppressed\n";
        });
    }

    // 在分配像素缓冲和启动线程前打开文件，失败时避免无效计算。
    std::ofstream out(filename);
    if (!out) {
        std::cerr << "无法写入 " << filename << "\n";
        return false;
    }
    out << "P3\n" << img_w << ' ' << img_h << "\n255\n";
    if (split) sw.start();           // 内部分段计时从写出 PPM 头之后开始

    // ### Tile 并行与缓冲所有权
    // ① 缓冲：**未量化**的线性 color（srgb/量化只在最后 write_color 里做一次）
    //    下标唯一：buf[j * img_w + i] ⇒ 每格恰好一个写者 ⇒ **缓冲本身不需要锁**
    // ② 调度共享状态 next_tile 由 mutex 保护；锁只覆盖领块，不覆盖像素计算。
    // ③ 全部 join() 之后主线程才读缓冲（join 是同步点 ⇒ happens-before）
    std::vector<color> buf(static_cast<std::size_t>(img_w) * static_cast<std::size_t>(img_h));
    const int tiles_x = (img_w + TILE - 1) / TILE;
    const int tiles_y = (img_h + TILE - 1) / TILE;
    const int n_tiles = tiles_x * tiles_y;

    std::mutex tile_mutex;
    int next_tile = 0;
    auto worker = [&]() {
        for (;;) {
            int t = 0;
            {
                std::lock_guard<std::mutex> lock(tile_mutex);   // 临界区只圈"领块"
                if (next_tile >= n_tiles) return;
                t = next_tile++;
            }
            const int tx = t % tiles_x;
            const int ty = t / tiles_x;
            const int i_end = std::min(tx * TILE + TILE, img_w);   // 钳位：不越界也不漏
            const int j_end = std::min(ty * TILE + TILE, img_h);
            for (int j = ty * TILE; j < j_end; ++j) {
                for (int i = tx * TILE; i < i_end; ++i) {
                    buf[static_cast<std::size_t>(j) * img_w + i] =
                        pixel_color(world, cam, i, j, spp, depth, base_seed);
                }
            }
        }
    };
    if (split) {
        split->open_to_first_pixel = sw.stop_ms();
        sw.start();
    }

    std::vector<std::thread> pool;
    pool.reserve(static_cast<std::size_t>(g_threads));
    for (int k = 0; k < g_threads; ++k) pool.emplace_back(worker);
    for (auto& th : pool) th.join();          // ← 同步点：此后 buf 才对主线程可见
    
    if (split) {
        split->first_pixel_to_join = sw.stop_ms();
        sw.start();
    }


    // 图像坐标 j 向上增长，PPM 从顶行写起：j 递减、i 递增。
    for (int j = img_h - 1; j >= 0; --j) {
        for (int i = 0; i < img_w; ++i) {
            write_color(out, buf[static_cast<std::size_t>(j) * img_w + i]);
        }
    }
    out.flush();                       // 显式推到文件；错误可能此时才浮现
    if (!out) {
        std::cerr << "写出 " << filename << " 时出错\n";
        return false;
    }
    if (split) {
        split->join_to_written = sw.stop_ms();
    }
    out.close();                       // 关闭失败也要报给调用方
    if (!out) {
        std::cerr << "关闭 " << filename << " 时出错\n";
        return false;
    }
    return true;
}

// 将毫秒计时结果写为 key=value；空路径不写，打开失败只警告。
// 这是计时数据输出，不代表图像写出成功；当前不检查计时文件后续写入状态。
static void write_timing(const std::string& path,
                         double prep_ms,
                         double render_all_ms,
                         const timing::RenderSplit& split) {
    if (path.empty()) return;
    std::ofstream tf(path);
    if (!tf) {
        std::cerr << "warning: 无法写入 timing 文件 " << path << "\n";
        return;
    }
    tf << "prep_ms="           << prep_ms                              << "\n";
    tf << "open_to_first_ms="  << split.open_to_first_pixel            << "\n";
    tf << "first_to_join_ms="  << split.first_pixel_to_join            << "\n";
    tf << "join_to_written_ms="<< split.join_to_written                << "\n";
    tf << "render_all_ms="     << render_all_ms                        << "\n";
}

int main(int argc, char** argv) {
    int spp = 1;
    int depth = 10;// --depth 默认 10（合法 1~32）
    int use_bvh = 0;                 
    double fuzz = 0.3;
    double ior = 1.5;
    uint64_t base_seed = 20260925ULL;
    std::string scene = "default";
    double light = 1.0;              // Cornell 光源强度
    std::string timing_path;
    timing::Stopwatch prep_sw;          // 准备段秒表（start() 在 argv 解析完成后）

    // argv 约定：argv[0] = 程序名本身 ⇒ 参数从 k = 1 开始。
    // 值用 `argv[++k]` 取：先自增再取值 ⇒ 值不会再被当成下一个参数。
    // 缺值、未知参数、解析或范围错误以退出码 2 报告；各选项按自己的类型解析。
    for (int k = 1; k < argc; ++k) {
        const std::string a = argv[k];
        auto need_val = [&](const char* name) -> const char* {
            if (k + 1 >= argc) {
                std::cerr << "缺少 " << name << " 的值\n";
                std::exit(2);
            }
            return argv[++k];
        };
        auto bad_val = [](const char* name, const char* raw) {
            std::cerr << name << " 的值非法: " << raw << "\n";
            std::exit(2);
        };

        long v = 0;
        if (a == "--width") {
            if (!parse_long(need_val("--width"),  v) || v < 1 || v > MAX_DIM) bad_val("--width",  argv[k]);   
            img_w = int(v);                                                          
        } else if (a == "--height") {
            if (!parse_long(need_val("--height"), v) || v < 1 || v > MAX_DIM) bad_val("--height", argv[k]); 
            img_h = int(v);                                                          
        } else if (a == "--spp") {
            if (!parse_long(need_val("--spp"),    v) || v < 1 || v > MAX_DIM) bad_val("--spp",    argv[k]);    
            spp = int(v);                                                            
        } else if (a == "--seed") {
            if (!parse_long(need_val("--seed"), v) || v < 0) bad_val("--seed", argv[k]);
            base_seed = uint64_t(v);
        } else if (a == "--fuzz") {
            if (!parse_double(need_val("--fuzz"), fuzz)) bad_val("--fuzz", argv[k]);
        } else if (a == "--ior") {
            if (!parse_double(need_val("--ior"), ior)) bad_val("--ior", argv[k]); 
        } else if (a == "--light") {                     // 光源强度
            if (!parse_double(need_val("--light"), light)) bad_val("--light", argv[k]);
        } else if (a == "--depth") {
            if (!parse_long(need_val("--depth"),  v) || v < 1 || v > 32)      bad_val("--depth",  argv[k]);   
            depth = int(v);                                                          
        } else if (a == "--bvh") {
            if (!parse_long(need_val("--bvh"), v))  bad_val("--bvh", argv[k]);
            use_bvh = int(v);
        }  else if (a == "--timing") {
                timing_path = need_val("--timing");     // ← 字符串直接取，不做数值校验
        }  else if (a == "--scene") {                    // 场景名白名单
            const char* raw = need_val("--scene");
            if (std::strcmp(raw, "cornell") != 0 &&
                std::strcmp(raw, "default") != 0 &&
                std::strcmp(raw, "checker") != 0 &&
                std::strcmp(raw, "exam") != 0) {
                bad_val("--scene", raw);
            }
            scene = raw;
        } else if (a == "--threads") {                   // 工作线程数
            // 缺值 ⇒ need_val 报错并 exit(2)；非整数/负数 ⇒ bad_val 走同一出口（rc=2）
            if (!parse_long(need_val("--threads"), v) || v < 0) bad_val("--threads", argv[k]);
            g_threads = int(v);
        } else {
            std::cerr << "未知参数: " << a << "\n";
            return 2;
        }
    }

    // Cornell 准备段：从解析完成后开始，包含范围检查、场景/BVH/相机构造及输出路径探测。
    if (!timing_path.empty()) prep_sw.start();

    // 运行时检查：尺寸 1~MAX_DIM，spp 1~MAX_DIM
    if (img_w < 1 || img_w > MAX_DIM || img_h < 1 || img_h > MAX_DIM) {
        std::cerr << "尺寸必须在 1~" << MAX_DIM << " 之间\n";
        return 2;
    }
    if (spp < 1 || spp > MAX_DIM) {
        std::cerr << "spp 必须在 1~" << MAX_DIM << " 之间\n";
        return 2;
    }
    if (depth < 1 || depth > 32) {
        std::cerr << "depth 必须在 1~32 之间\n";
        return 2;
    }
    if (fuzz < 0.0 || fuzz > 1.0) {
        std::cerr << "fuzz 必须在 [0,1] 之间\n";
        return 2;
    }
    // 折射率必须为正，避免求倒数时除零；parse_double 已拒绝非有限输入。
    if (ior < MIN_IOR || ior > MAX_IOR) {
        std::cerr << "ior 必须在 [" << MIN_IOR << ", " << MAX_IOR << "] 之间（玻璃 ≈ 1.5）\n";
        return 2;
    }
    // 光源强度仅用于 Cornell；(0,1e9] 是本程序的输入限制，不是物理范围。
    if (!(light > 0.0) || light > 1e9) {
        std::cerr << "--light 必须在 (0, 1e9] 之间（默认 1.0）\n";
        return 2;
    }
    if (use_bvh!=0&&use_bvh!=1) {
        std::cerr << "bvh必须为0或者1\n";
        return 2;
    }
    // 至少启动一个工作线程，否则缓冲不会被计算。
    // 当前未设上限；超出块数的线程可能领不到任务，但仍有创建开销与资源失败风险。
    if (g_threads < 1) {
        std::cerr << "--threads 必须 >= 1（默认 1）\n";
        return 2;
    }

    double aspect = double(img_w) / double(img_h);

    // SUP-02 考官分支：与渲染路径无关，不写图、不动默认 12 图行为。
    // 成功返回 0；任一期望不符返回 2（与 main 已过返回契约一致）。
    if (scene == "exam") {
        int failures = 0;
        interval win(0.001, std::numeric_limits<double>::infinity());
        std::shared_ptr<const material> mat = std::make_shared<lambertian>(color(0.5, 0.5, 0.5));
        ray r(point3(0,0,0), vec3(0,0,-1));
        const double nan = std::numeric_limits<double>::quiet_NaN();
            // 第 5 条：区间语义（对 src/interval.h 再跑一次）
            std::printf("surrounds(0.001)=%d contains(0.001)=%d surrounds(nan)=%d contains(nan)=%d\n",
            win.surrounds(0.001),win.contains(0.001),win.surrounds(nan),win.contains(nan)
            );

            // 第 6 条：同一个 rec，先正常命中、再相切命中
            hit_record rec;
            sphere A(point3(0,0,-3), 1.0,mat);
             const bool ok_A = A.hit(r, win, rec);

            // A 的期望：命中、t=2（绝对误差<=1e-12）、front_face=true
            const bool   expect_hit_A   = true;
            const bool   expect_front_A = true;
            const double expect_t_A     = 2.0;

            bool a_ok = (ok_A == expect_hit_A);
            if (a_ok && ok_A) {
                // 先确认命中，再读 rec 的字段
                a_ok = std::isfinite(rec.t)
                    && std::abs(rec.t - expect_t_A) <= 1e-12
                    && (rec.front_face == expect_front_A);
            }
            if (!a_ok) {
                ++failures;
                std::printf("FAIL A\n");
            } else {
                std::printf("PASS A  t=%.17g  front_face=%d\n", rec.t, rec.front_face ? 1 : 0);
            }
            const hit_record after_A = rec;

            sphere C(point3(0,0,3), 1.0, mat);      // 相机后
            const bool ok_C = C.hit(r, win, rec);

            // ② 期望1：C 必须 miss
            const bool   expect_hit_C   = false;
            bool c_ok = (ok_C == expect_hit_C);

            // ③ 期望2：miss 之后 rec 未被改动（与快照一致）
            if (c_ok) {
                const bool t_same  = (rec.t == after_A.t);
                const bool ff_same = (rec.front_face == after_A.front_face);
                const bool p_same  = (rec.p.x() == after_A.p.x())
                                  && (rec.p.y() == after_A.p.y())
                                  && (rec.p.z() == after_A.p.z());
                const bool n_same  = (rec.normal.x() == after_A.normal.x())
                                  && (rec.normal.y() == after_A.normal.y())
                                  && (rec.normal.z() == after_A.normal.z());
                c_ok = t_same && ff_same && p_same && n_same;
            }

            // ④ 累计与日志：沿用 A 的写法（++failures / FAIL 文案）
            if (!c_ok) { ++failures; std::printf("FAIL C\n");}
            else       {  std::printf("PASS C  t=%.17g  front_face=%d  p=(%.1f,%.1f,%.1f)\n",
                                     rec.t, rec.front_face ? 1 : 0,
                                     rec.p.x(), rec.p.y(), rec.p.z()); }

            sphere B(point3(0,1,-3), 1.0, mat);
            const bool ok_B = B.hit(r, win, rec);

            const bool   expect_hit_B   = true;
            const double expect_t_B     = 3.0;
            const bool   expect_front_B = false;      // 切触 dot==0 归背面

            bool b_ok = (ok_B == expect_hit_B);
            if (b_ok) {
                b_ok = std::isfinite(rec.t)
                    && std::abs(rec.t - expect_t_B) <= 1e-12
                    && (rec.front_face == expect_front_B);
            }
            if (!b_ok) {  ++failures; std::printf("FAIL B\n");}
            else       { std::printf("PASS B  t=%.17g  front_face=%d\n", rec.t, rec.front_face ? 1 : 0);}

            const vec3 up = unit_vector(vec3(0,1,-1));

            struct ICase { const char* tag; double x; bool c; bool s; };
            const ICase icases[] = {
                {"正常用例",      5.0,  true,true},
                {"闭 vs 开的差别",0.001,true,false},
                {"边界外",        0.0,  false,false},
                {"非有限输入 fail-closed", std::numeric_limits<double>::quiet_NaN(),  false,false}
            };
            for (const auto& ic : icases) {
                const bool c = win.contains(ic.x);
                const bool s = win.surrounds(ic.x);
                const bool ok = (c == ic.c) && (s == ic.s);
                if (!ok) { ++failures; std::printf("FAIL interval %s\n", ic.tag); }
                else     { std::printf("PASS interval %s\n", ic.tag); }
            }

            struct P { const char* tag; sphere s; ray rr; bool e_ok; double e_t; bool e_ff; bool has_t; };
            const P ps[] = {
                {"球在相机后", sphere(point3(0,0,3), 1.0, mat), r,                       false, 0.0, false, false},
                {"相切",       sphere(point3(0,1,-3), 1.0, mat), r,                      true,    3.0, false, true },
                {"相机在球内", sphere(point3(0,0,-3), 5.0, mat), r,                       true,    8.0, false, true },
                {"打不中",     sphere(point3(0,0,-3), 1.0, mat), ray(point3(0,0,0), up),  false, 0.0, false, false},
            };
            for (const auto& p : ps) {
                hit_record rc;
                const bool ok = p.s.hit(p.rr, win, rc);
                bool pass = (ok == p.e_ok);
                if (pass && ok && p.has_t) {
                    pass =  std::isfinite(rc.t)
                            && std::abs(rc.t - p.e_t) <= 1e-12
                            && (rc.front_face == p.e_ff);
                }
                if (!pass) { ++failures; std::printf("FAIL probe %s\n", p.tag); }
                else       { std::printf("PASS probe %s  ok=%d%s\n", p.tag, ok ? 1 : 0, ok ? "" : ""); }
            }
        std::cout << "exam checks done: failures=" << failures << "\n";
        return failures == 0 ? 0 : 2;
    }

    
    
    if (scene == "cornell") {
        // ---- Cornell 相机 ----
        const point3 lookfrom(0.0, 2.0,  8.0);   // 眼睛
        const point3 lookat (0.0, 2.0, -2.0);    // 后墙中心
        const vec3   up     (0.0, 1.0,  0.0);
        const double vfov   = 40.0;              // 想更近就减小，想看到更多墙就加大

        // ---- 场景：bvh 开关复用同一套分派；光源强度走 --light（默认 1.0）----
        world w = use_bvh ? world::with_bvh(cornell_scene(light))
                        : cornell_scene(light);

        // ---- 相机：与 multi_scene 用同一个工厂，只是参数不同 ----
        camera cam = camera::from_lookat_vfov(aspect, vfov, lookfrom, lookat, up);

        // 优先 out/cornell.ppm；打不开时回退到 cwd 中的 cornell.ppm。
        // 两条路径都相对于 cwd；回退路径也可能不可写，不保证一定生成产物。
        // 探测使用 ofstream，会创建或截断目标；调用者应选择独立的运行目录。
        std::string out_path = "out/cornell.ppm";
        {
            std::ofstream probe(out_path);
            if (!probe) {
                out_path = "cornell.ppm";
                std::cerr << "info: out/ 不可写，改写到 ./cornell.ppm\n";
            }
        }
        // 准备段到此结束；render_all 覆盖完整 render 调用（含文件关闭），内部分段不含全部开销。
        const double prep_ms = timing_path.empty() ? 0.0 : prep_sw.stop_ms();
        timing::RenderSplit split;
        timing::Stopwatch whole;
        whole.start();
        if (!render(w, cam, spp, depth, base_seed, out_path,
               timing_path.empty() ? nullptr : &split)) return 3;
        const double render_all_ms = whole.stop_ms();
        if (!timing_path.empty()) write_timing(timing_path, prep_ms, render_all_ms, split);

        // 仅在写出成功后报告一张输出；失败时不打印成功声明。
        // 模板与默认路径**逐字一致**（"done: N images, spp=… ior=…"），只换数字
        std::cout << "done: " << 1 << " images, spp=" << spp << " ior=" << ior << "\n";
        std::cout << "mode: " << (use_bvh ? "bvh" : "brute") << "\n";
        return 0;
    }

    if (scene == "checker") {
    // 棋盘场景机位：略微俯视球心，垂直 FOV 为 35 度。
    const point3 lookfrom(0.0, 0.6,  1.6);
    const point3 lookat (0.0, 0.0, -1.0);
    const vec3   up     (0.0, 1.0,  0.0);
    const double vfov   = 35.0;

    // ---- 纹理：构造期一次性生成 64×64 纹素；之后只读 ----
    // 直接声明为 shared_ptr<const texture>：
    //   "这是个纹理，不是颜色"的意图在声明处一步写清，不需要 static_pointer_cast。
    std::shared_ptr<const texture> tex = std::make_shared<checker_texture>(64);

    // ---- 场景：只有一颗带棋盘纹理的球；bvh 开关复用同一分派 ----
    world w = [&] {
        world base;
        base.add(std::make_shared<sphere>(
            point3(0.0, 0.0, -1.0), 0.5,
            std::make_shared<lambertian>(tex)));
        return use_bvh ? world::with_bvh(std::move(base)) : std::move(base);
    }();

    // ---- 相机：与 cornell 用同一个工厂，只是参数不同 ----
    camera cam = camera::from_lookat_vfov(aspect, vfov, lookfrom, lookat, up);

    // 输出隔离：先试 out/checker.ppm；打不开就退回 cwd 的 checker.ppm（与 cornell 同款）
    std::string out_path = "out/checker.ppm";
    {
        std::ofstream probe(out_path);
        if (!probe) {
            out_path = "checker.ppm";
            std::cerr << "info: out/ 不可写，改写到 ./checker.ppm\n";
        }
    }
    if (!render(w, cam, spp, depth, base_seed, out_path)) return 3;   // checker 不写计时数据，即使提供了 --timing

    // done 行与 default/cornell 逐字一致，只换数字
    std::cout << "done: " << 1 << " images, spp=" << spp << " ior=" << ior << "\n";
    std::cout << "mode: " << (use_bvh ? "bvh" : "brute") << "\n";
    return 0;
    }

    // 文件名后缀：spp==1 保留既有文件名，否则追加 _sppN；尺寸与 seed 不写入名称。
    std::string suffix = (spp == 1) ? "" : ("_spp" + std::to_string(spp));
    camera cam(aspect, 1.0);
    if (!render(use_bvh ? world::with_bvh(pair_scene(fuzz)) : pair_scene(fuzz), cam, spp, depth, base_seed,
       "pair_diffuse_metal_fuzz" + std::to_string(int(fuzz * 100)) + suffix + ".ppm")) return 3;
    if (!render(use_bvh ? world::with_bvh(metal_scene(fuzz)) : metal_scene(fuzz), cam, spp, depth, base_seed,
       "metal_fuzz" + std::to_string(int(fuzz * 100)) + suffix + ".ppm")) return 3;
    if (!render(use_bvh ? world::with_bvh(base_scene()) : base_scene(), cam, spp, depth, base_seed,
           "test_sphere" + suffix + ".ppm")) return 3;
    if (!render(use_bvh ? world::with_bvh(multi_scene()) : multi_scene(), cam, spp, depth, base_seed,
           "multi_sphere" + suffix + ".ppm")) return 3;
    if (!render(use_bvh ? world::with_bvh(base_scene()) : base_scene(), camera::from_vfov(aspect, 45.0), spp, depth, base_seed,
           "cam_fov45" + suffix + ".ppm")) return 3;
    if (!render(use_bvh ? world::with_bvh(base_scene()) : base_scene(), camera::from_vfov(aspect, 60.0), spp, depth, base_seed,
           "cam_fov60" + suffix + ".ppm")) return 3;
    if (!render(use_bvh ? world::with_bvh(base_scene()) : base_scene(), camera::from_vfov(aspect, 90.0), spp, depth, base_seed,
           "cam_fov90" + suffix + ".ppm")) return 3;
    if (!render(glass_scene(ior), cam, spp, depth, base_seed,
       "glass_ior" + std::to_string(int(ior * 100)) + suffix + ".ppm")) return 3;

    // 同一多球场景的四个机位：左移、抬高、水平转向、垂直俯视。
    // 垂直俯视时 up 与视线平行，相机使用回退轴确定画面朝上方向。
    {
        const point3 target(0.0, 0.0, -3.0);                 // multi_scene 的中心球
        const vec3   up(0.0, 1.0, 0.0);
        const double vfov = 53.13010235415598;               // = 旧默认 vh=1.0 对应的 vfov
        if (!render(use_bvh ? world::with_bvh(multi_scene()) : multi_scene(),
               camera::from_lookat_vfov(aspect, vfov, point3(-2.6, 0.6, 0.6), target, up),
               spp, depth, base_seed, "cam_move_left" + suffix + ".ppm")) return 3;
        if (!render(use_bvh ? world::with_bvh(multi_scene()) : multi_scene(),
               camera::from_lookat_vfov(aspect, vfov, point3(0.0, 2.2, 1.4), target, up),
               spp, depth, base_seed, "cam_move_high" + suffix + ".ppm")) return 3;
        if (!render(use_bvh ? world::with_bvh(multi_scene()) : multi_scene(),
               camera::from_lookat_vfov(aspect, vfov, point3(0.0, 0.0, 0.0),
                                        point3(-1.5, 0.0, -2.598076211353316), up),
               spp, depth, base_seed, "cam_turn30" + suffix + ".ppm")) return 3;
        if (!render(use_bvh ? world::with_bvh(multi_scene()) : multi_scene(),
               camera::from_lookat_vfov(aspect, vfov, point3(0.0, 4.2, -3.0), target, up),
               spp, depth, base_seed, "cam_look_down" + suffix + ".ppm")) return 3;
    }
    // 保持既有 done: 输出格式，mode: 单独占一行，便于脚本分别读取。
    // done: 仅在全部 12 张都写出成功后才打印；失败时以 stderr 诊断并返回 3，已写出的图不回滚删除。
    std::cout << "done: 12 images, spp=" << spp << " ior=" << ior << "\n";
    if(use_bvh == 1){std::cout << "mode: bvh" << "\n";}
    else if(use_bvh == 0){std::cout << "mode: brute" << "\n";}
    // default 不生成计时文件；仅在请求 --timing 时向 stderr 警告。
    // 当前 checker 也未接计时；下面诊断文字的范围比实际实现宽。
    if (!timing_path.empty()) {
        std::cerr << "warning: --timing 仅对 --scene cornell/checker 生效；default 路径（12 张图）不计时\n";
    }
    return 0;
}