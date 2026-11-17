#pragma once
#include"hittable.h"
#include<vector>
#include<memory>
#include<utility>

class hittable_list:public hittable
{
    private:
    std::vector<std::shared_ptr<hittable>> objects;
    public:
    // 空集合可构造：hit 返回 false 且不改 rec，bounding_box 返回空盒。
    // add 接收共享所有权句柄；调用者保证 object 非空，函数不检查。
    // clear 释放本集合持有的句柄；其他共享所有者仍可保留对象。
    // hit 遵循基类的开区间、最近命中及失败不改 rec 契约。
    // 每次命中收缩上界，等 t 命中不替换先遇到的对象。
    // 对象及其材质必须在使用借用的命中记录期间存活。
    hittable_list() = default;
    void  add(std::shared_ptr<hittable> object){
        objects.emplace_back(std::move(object));
    }
    void clear(){
        objects.clear();
    }
    bool hit(const ray& r,const interval& ray_t,hit_record& rec) const override{
        hit_record temp_rec;
        bool hit_anything = false;
        double closest_so_far = ray_t.max; 
        for(const auto& object:objects){
            if(object->hit(r,interval(ray_t.min, closest_so_far),temp_rec)){
                hit_anything = true;
                rec = temp_rec;
                closest_so_far = temp_rec.t;
            }
        }
        return hit_anything;
    }
    aabb bounding_box() const override{
        aabb temp;
        for(const auto& object:objects){
            temp.surround_box(object->bounding_box());
        } 
        return temp;
    }

    bool has_emissive() const noexcept override {
        for (const auto& obj : objects) {
            if (obj && obj->has_emissive()) return true;
        }
        return false;
    }
};
