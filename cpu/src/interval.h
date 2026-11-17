#pragma once
#include <limits>

struct interval {
    double min, max;

    interval() : min(+std::numeric_limits<double>::infinity()),
                 max(-std::numeric_limits<double>::infinity()) {}   // 空区间
    // 原样保存，不排序；a>b 表示空，a==b 是闭区间单点、开区间空集。
    interval(double a, double b) : min(a), max(b) {}

    double size() const { return max - min; }
    bool contains (double x) const { return min <= x && x <= max; }   // 闭区间（clamp / 取值域用）
    bool surrounds(double x) const { return min <  x && x <  max; }   // 开区间（求交窗口用）
    
    // 调用者保证 min<=max 且端点和 x 无 NaN；此处不验证，NaN x 会原样返回。
    double clamp(double x) const { 
        if (x < min) return min; 
        if (x > max) return max; 
        return x; }
};
