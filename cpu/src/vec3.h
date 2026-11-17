#pragma once
#include<iostream>
#include<cmath>

struct vec3
{
    // 值语义三分量；索引调用者保证 0..2，除法保证有限非零除数。
    double v[3];
    vec3():v{0,0,0}{}
    vec3(double x,double y,double z):v{x,y,z}{}
    
    double x() const{return v[0];}
    double y() const{return v[1];}
    double z() const{return v[2];}

    vec3 operator-() const{return {-v[0],-v[1],-v[2]};}
    double operator[](int i) const{return v[i]; }
    double& operator[](int i){return v[i];}

    vec3& operator+=(const vec3& a){
        v[0]+=a[0];v[1]+=a[1];v[2]+=a[2];
        return *this;
    }
    vec3& operator-=(const vec3& a){
        v[0]-=a[0];v[1]-=a[1];v[2]-=a[2];
        return *this;
    }

    vec3& operator*=(double t){v[0]*=t;v[1]*=t;v[2]*=t;return *this;}
    vec3& operator/=(double t){ return *this *= (1.0/t); }
    double length_squared() const{return v[0]*v[0]+v[1]*v[1]+v[2]*v[2];}
    double length() const{return std::sqrt(length_squared());}
    // 逐分量绝对阈值判断，不是长度阈值，也不是非有限值检查。
    bool near_zero() const {
    const double s = 1e-8;
    return (std::fabs(v[0]) < s) && (std::fabs(v[1]) < s) && (std::fabs(v[2]) < s);
}

};

inline vec3 operator+(const vec3& a,const vec3& b){
    return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};
}
inline std::ostream& operator<<(std::ostream& out,const vec3& a){
    return out<<a[0]<<' '<<a[1]<<' '<< a[2];
}

inline vec3 operator*(const vec3& a,double t){
    return {a[0]*t,a[1]*t,a[2]*t};
}

inline vec3 operator* (double t,const vec3& a){
    return {a[0]*t,a[1]*t,a[2]*t};
}

// 分量乘（Hadamard）：颜色×颜色 / 权重×颜色 需要它
//   用途：ray_color 里 throughput = throughput * attenuation、以及 throughput * background(r)
inline vec3 operator*(const vec3& a,const vec3& b){
    return {a[0]*b[0], a[1]*b[1], a[2]*b[2]};
}

inline vec3 operator/(const vec3& a,const double t){
    return a*(1/t);
}

// 调用者保证向量有限、长度有限且非零；无退化检查或回退方向。
inline vec3 unit_vector(const vec3& v){
    return v/v.length();
}
inline vec3   operator-(const vec3& u, const vec3& v){ return {u[0]-v[0], u[1]-v[1], u[2]-v[2]}; }
inline double dot(const vec3& u, const vec3& v){ return u[0]*v[0] + u[1]*v[1] + u[2]*v[2]; }
inline vec3   cross(const vec3& u, const vec3& v){
    return { u[1]*v[2]-u[2]*v[1],
             u[2]*v[0]-u[0]*v[2],
             u[0]*v[1]-u[1]*v[0] };
}

using point3 = vec3;
using color = vec3;

