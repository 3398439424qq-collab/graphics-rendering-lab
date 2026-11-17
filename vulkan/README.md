# Vulkan renderer

C++17 / Vulkan 渲染学习工程的当前源码快照。代码覆盖窗口与设备初始化、交换链与帧同步、资源上传、OBJ 网格、纹理、MVP / 深度、法线显示以及直接光照。公开整理保留应用和着色器实现，只整理构建入口、示例配置及说明。

## 当前范围

- GLFW 窗口、交换链重建、最小化恢复与资源生命周期。
- 顶点 / 索引缓冲、2×2 程序纹理、固定演示 OBJ 网格。
- 透视投影、深度测试、世界空间法线显示、A / D 水平移动相机。
- GGX 分布、相关 Smith 几何项和 Schlick Fresnel 的限定直接光照实现。
- 场景文本配置读写与单一材质 tint 接线。

截至 2026-11-17，直接光照阶段已有原工程验证记录；材质与颜色空间阶段仍在进行。当前材质接线只有离线检查，真实 GPU 材质消费、sRGB 中间色与完整参数展示尚未验收。本次公开副本已完成离线 Release 构建，范围见[快照与验证说明](../docs/SNAPSHOT.md)。

## 支持范围与依赖

本快照的构建入口支持 **原生 Windows、MSVC、x64、C++17**。需要：

- CMake 3.24 或更新版本。
- Visual Studio 2022 的 C++ 桌面开发工具与 Windows SDK。
- Vulkan SDK，包含头文件、loader 导入库和 `glslc`。
- GLFW 的 MSVC x64 开发包，或可被 CMake 找到的 `glfw3` 配置包。显式开发包使用静态 `glfw3.lib`，须与 `/MD` 运行库兼容。

原工程依赖基线为 Vulkan SDK 1.4.363.0、GLFW 3.5.1；其他依赖版本未逐一验证。tinyobjloader 头文件与其[完整许可证（含 MIT / ISC 段）](third_party/tinyobjloader/LICENSE)已随源码保留，演示 OBJ 位于 `assets/`。

**运行限制：** 当前设备选择逻辑固定选择 NVIDIA 独立显卡。窗口路径还要求 `VK_EXT_surface_maintenance1`、`VK_EXT_swapchain_maintenance1` 及使用到的维护特性支持。其他 GPU 厂商、缺少扩展的驱动和 Linux / WSL 图形运行不在本快照已验证范围内。无参数诊断也会调用 Vulkan；它不是纯 CPU 测试。

## 构建

在仓库根目录打开 PowerShell。将下方占位路径替换成自己的安装目录：

```powershell
cmake -S vulkan -B build/vulkan -G "Visual Studio 17 2022" -A x64 -T host=x64 `
  -DVK01_SDK_ROOT="C:/path/to/VulkanSDK" `
  -DVK01_GLFW_ROOT="C:/path/to/glfw"
cmake --build build/vulkan --config Release --parallel
```

SDK 根目录应包含 `Include/vulkan/vulkan.h`、`Lib/vulkan-1.lib`、`Bin/glslc.exe`。GLFW 根目录应包含 `include/GLFW/glfw3.h`，以及 `lib-vc2022/glfw3.lib` 或 `lib/glfw3.lib`。

如果 Vulkan SDK 已通过 `VULKAN_SDK` 环境变量或 CMake 默认搜索路径可见，可以省略 `VK01_SDK_ROOT`。如果 GLFW 已提供 CMake 配置包，可以省略 `VK01_GLFW_ROOT`，改用 `CMAKE_PREFIX_PATH` 或 `glfw3_DIR` 定位。更换依赖位置时使用新的构建目录，以免沿用旧缓存。

构建会编译 30 个 C++ 翻译单元和 9 个 GLSL 着色器，并将模型与示例配置放入构建目录。构建过程不启动应用。

## 源码存放在 WSL 时

源码可以一直保留在 WSL 中。仍使用原生 Windows 编译器，通过 UNC 路径读取源码，并把构建产物放在可写的 Windows 本地目录，不必复制第二份源码。

下面的 WSL 用户和项目路径是占位值；替换后在 Windows PowerShell 中执行：

```powershell
$sourceRoot = '\\wsl.localhost\Ubuntu-24.04\home\YOUR_USER\YOUR_PROJECT\graphics-rendering-lab\vulkan'
$buildRoot = Join-Path $env:LOCALAPPDATA 'graphics-rendering-lab-build/vulkan'
cmake -S $sourceRoot -B $buildRoot -G "Visual Studio 17 2022" -A x64 -T host=x64 `
  -DVK01_SDK_ROOT="C:/path/to/VulkanSDK" `
  -DVK01_GLFW_ROOT="C:/path/to/glfw"
cmake --build $buildRoot --config Release --clean-first
Set-Location $buildRoot
```

本次已从 WSL 源码位置完成上述离线构建。MSBuild 对 WSL UNC 路径出现过 `MSB8064` 大小写依赖跟踪警告，因此这条路径下不要依赖增量检测；源码变更后重新配置，并使用 `--clean-first` 做完整构建。警告不应被当成已验证增量构建正常的证据。

## 运行与交互

**必须从构建目录运行**，因为程序按当前目录读取 `assets/` 和 `shaders/`；只把 EXE 拷到别处会丢失资源。

```powershell
Set-Location build/vulkan
./Release/vk01.exe --window-mvp
```

| 参数 | 用途 |
|---|---|
| 无参数 | 枚举设备与队列，尝试创建并释放逻辑设备 |
| `--maintenance-check` | 查询并尝试启用维护扩展所需能力 |
| `--window` | 固定网格与纹理显示 |
| `--window-mvp` | MVP、深度与当前直接光照路径 |
| `--window-normal` | 世界空间法线显示 |

在 MVP / 法线窗口中，按住 **A / D** 水平移动相机；通过窗口关闭按钮退出。窗口模式会运行 GPU。

`--window-mvp` 和 `--window-normal` 支持场景配置，例如：

```powershell
./Release/vk01.exe --window-mvp --scene assets/example.scene
./Release/vk01.exe --window-normal --scene assets/example.scene --save-scene saved.scene
```

`assets/example.scene` 使用与源码默认值一致的 13 个字段。每行格式是 `字段名 数值`，允许空行，不支持注释；字段名不能重复。材质 tint 和光强范围为 0 到 1，光方向必须非零。`--save-scene` 会覆盖指定文件，然后继续进入窗口模式。

也可以用 `--projection <path>` 单独加载只含 `fov_y_radians`、`z_near`、`z_far` 的投影文件。`--projection` 与 `--scene` 二选一；如果同时保存，`--save-scene` 必须放在加载参数之后。

## 已知限制

- 当前直接光照着色器将微表面参数 `alpha` 固定为 1.0，将 `F0` 固定为 0.04；没有开放完整金属度 / 粗糙度材质参数。
- 演示模型路径固定为 `assets/demo_mesh.obj`，纹理为内置 2×2 像素；当前没有任意模型 / 纹理文件的命令行加载接口。
- 材质与颜色空间仍是开发中的功能，不把当前离线编译结果视为完整视觉验证。
- 原工程的历史教学考官、冻结参考实现和运行日志没有整套带入公开目录；本目录不提供完整自动化 GPU 回归套件。

源码中的已有作者 / AI 辅助注释予以保留，来源和许可范围以仓库根目录说明为准。
