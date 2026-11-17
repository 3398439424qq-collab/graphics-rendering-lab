#pragma once
#include"vec3.h"

class ray{
    private:
        vec3 dir;
        point3 orig;
    public:
        // 默认原点与方向均为零，仅用于随后赋值的占位，不可直接用于求交。
        ray(){}
        // 按值保存，不自动归一化或验证方向；求交调用者保证有限且非零。
        ray(const point3& orig,const vec3& dir):dir{dir},orig{orig}{};

        vec3 direction() const{return dir;}
        point3 origin() const{return orig;}

        // t 是射线参数；位移长度为 |t|*|dir|，仅单位方向时才对应距离大小。
        point3 at(double t) const {return orig+dir*t;}

};