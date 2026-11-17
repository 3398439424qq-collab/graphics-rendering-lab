# 来源、贡献与第三方许可

## 项目与协作来源

本项目是作者的图形学习与工程实践。代码开发包含本人实现，以及 AI 提供的教学、接口支架、局部辅助实现、验证工具、机械接线与文档整理。原源码中的具体归属说明保留；公开整理不会将辅助贡献改称独立完成。

2026-11-17 的发布准备在独立副本中完成，包括筛选文件、整理 CMake、编写使用说明与选择展示图。公开目录不包含原学习仓库的笔记、练习、教师参考材料、完整验证档案或旧 Git 历史。

## Ray Tracing in One Weekend

- CPU 渲染器以该系列为学习基础，逐步重写与扩展。
- 作者与维护者包括 Peter Shirley、Trevor David Black 和 Steve Hollasch。
- 官方入口：[raytracing.github.io](https://raytracing.github.io/)。
- 官方源码：[RayTracing/raytracing.github.io](https://github.com/RayTracing/raytracing.github.io)。
- 上游许可：CC0 1.0 Universal，见官方 [COPYING.txt](https://github.com/RayTracing/raytracing.github.io/blob/release/COPYING.txt)。

原学习记录没有固定唯一的上游提交版本，本仓库不猜测该版本。CPU 的模糊金属模型属于学习用近似，不是 Vulkan 模块中的 GGX 模型。

## tinyobjloader

`vulkan/third_party/tinyobjloader/tiny_obj_loader.h` 是随源码分发的第三方依赖，用于读取 OBJ。

保留[完整 LICENSE](../vulkan/third_party/tinyobjloader/LICENSE)及头文件内声明。原许可证文件包含 tinyobjloader 的 MIT 许可和 mapbox/earcut.hpp 的 ISC 许可段；本次没有删节或重新授权这些内容。

## 外部构建依赖

Vulkan SDK 和 GLFW 由使用者在本地安装并配置，本仓库不打包它们的 SDK、头文件、静态库或运行库。其许可与分发条件由各上游项目管理。

## 展示资源

- 三张 CPU PNG 来自作者原学习工程保存的 2026-10-14 代表输出，保持原文件字节不变。
- `vulkan/assets/demo_mesh.obj` 是项目中已有的固定 AI 辅助示例资源，未替换为外部模型。

## 整体项目许可证

整体项目许可证尚待作者确认。本次整理没有代替作者选择 MIT、Apache-2.0 或其他许可证。上述第三方许可仅描述其各自组件，不自动构成对整个项目的统一授权。
