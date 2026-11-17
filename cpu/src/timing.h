#pragma once
#include <chrono>

namespace timing {
    class Stopwatch {
    public:
    void start() {
        start_ = std::chrono::steady_clock::now();
        running_ = true;
    }

    // 语义：未 start() 或已 stop 时返回 0.0；start() 可重置起点，首次 stop 返回间隔毫秒数。
    //   0.0 也可能是实际零间隔；返回值及默认字段没有有效位，须结合调用流程判断是否计时。
    double stop_ms() {
        if (!running_) return 0.0;
        running_ = false;
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

    private:
        std::chrono::steady_clock::time_point start_{};
        bool running_ = false;
    };
struct RenderSplit {
    // 三段间隔，单位 ms；字段名保留，实际边界由 render 打点决定，默认 0 不表示已测量。
    double open_to_first_pixel = 0.0;   // ofstream 打开且 PPM 头写入后开表 → 创建工作线程前；不含打开与写头
    double first_pixel_to_join = 0.0;   // 创建工作线程前开表 → 全部 join 后；并非首次 pixel_color 锚点
    double join_to_written     = 0.0;   // join 后开表 → buf 写出并检查流状态；不保证涵盖关闭或磁盘落盘
    };
}