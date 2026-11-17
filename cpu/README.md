# CPU 路径追踪渲染器

C++17 学习项目：从射线与几何求交，到材质散射、相机、BVH 和 tile 多线程渲染。场景与棋盘纹理由程序生成，运行时不需要外部模型或纹理。

本目录是当前开发源码的公开快照。18 个源文件保持导出时的内容，构建入口与本说明为公开展示重新整理。项目来源和第三方说明见仓库根目录。

## 当前功能

- 球、平行四边形；漫反射、模糊金属、介质和单面发光材质。
- 相机位置、朝向与视场角示例；固定种子的逐像素采样。
- AABB / BVH，可切换 BVH 与逐物体遍历。
- 32 × 32 tile 分发与多线程渲染，默认使用一个工作线程。
- PPM 图像输出；内置几何/区间自检；Cornell 场景分段计时。

## 构建

需要 CMake 3.20 或更高版本，以及支持 C++17 的编译器和线程库。以下命令从仓库根目录执行。

### Linux / macOS

```sh
cmake -S cpu -B build/cpu -DCMAKE_BUILD_TYPE=Release
cmake --build build/cpu --parallel
ctest --test-dir build/cpu --output-on-failure
```

可执行文件为 `build/cpu/rt`。构建配置支持 GCC / Clang；此次公开整理的实际验证环境见文末，其他平台不据此视为已验证。

### Windows：MinGW-w64

在已将 MinGW-w64 的 `bin` 加入 PATH 的 PowerShell 中执行：

```powershell
cmake -S cpu -B build/cpu-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/cpu-mingw --parallel
ctest --test-dir build/cpu-mingw --output-on-failure
```

可执行文件为 `build/cpu-mingw/rt.exe`。

### Windows：Visual Studio

在安装了 C++ 桌面开发工具的环境中执行：

```powershell
cmake -S cpu -B build/cpu-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build/cpu-msvc --config Release --parallel
ctest --test-dir build/cpu-msvc -C Release --output-on-failure
```

可执行文件为 `build/cpu-msvc/Release/rt.exe`。此生成器入口尚未在本次公开整理中验证。

## 运行

输出路径相对于**运行时工作目录**。同名输出会被覆盖，因此示例在独立的 `renders` 目录运行。

Linux / macOS 示例：

```sh
mkdir -p renders/out
cd renders
../build/cpu/rt --scene cornell --width 400 --height 400 --spp 64 --depth 10 --seed 20260925 --bvh 1 --threads 4
```

Windows MinGW-w64 示例：

```powershell
New-Item -ItemType Directory -Force renders/out | Out-Null
Set-Location renders
../build/cpu-mingw/rt.exe --scene cornell --width 400 --height 400 --spp 64 --depth 10 --seed 20260925 --bvh 1 --threads 4
```

结果写入 `renders/out/cornell.ppm`。如果当前工作目录下的 `out` 不可写，程序会尝试写入当前目录的 `cornell.ppm`。使用支持 PPM 的图像查看器打开；展示用 PNG 与来源说明放在仓库展示目录。

### 场景

| `--scene` | 行为 |
|---|---|
| `default` | 输出 12 张材质与相机对照图，直接写入当前目录；`spp` 不为 1 时在文件名添加 `_sppN` |
| `cornell` | 输出一张 Cornell 场景图；支持光源强度及分段计时 |
| `checker` | 输出一张程序化棋盘纹理球图 |
| `exam` | 执行内置几何/区间自检，不生成图像；全部通过时退出码为 0 |

### 参数

| 参数 | 默认值 | 说明 |
|---|---:|---|
| `--width` / `--height` | 400 / 225 | 每维允许 1–8192 |
| `--spp` | 1 | 每像素样本数，允许 1–8192 |
| `--depth` | 10 | 路径递归深度，允许 1–32 |
| `--seed` | 20260925 | 非负整数；解析范围取决于平台的 `long` |
| `--bvh` | 0 | 0：逐物体遍历；1：对支持该选项的场景启用 BVH |
| `--threads` | 1 | 正整数；建议使用适合本机资源的小值 |
| `--fuzz` | 0.3 | 默认场景中的金属模糊参数，允许 0–1 |
| `--ior` | 1.5 | 默认场景中的介质折射率，演示输入范围 0.1–10 |
| `--light` | 1.0 | Cornell 光源强度，允许大于 0 且不超过 1e9 |
| `--timing` | 不写文件 | 计时输出路径；当前实际只在 `cornell` 场景生成 |

例如在上述 `renders` 目录中追加计时输出：

```sh
../build/cpu/rt --scene cornell --width 128 --height 128 --spp 8 --bvh 1 --threads 4 --timing cornell-timing.txt
```

没有 `--help` 选项；未知参数、缺值或非法值返回退出码 2。图像写入失败返回退出码 3。

## 验证与范围

`ctest` 调用现有 `rt --scene exam`，覆盖球求交、相切、相机在球内、未命中记录保持、区间端点和 NaN 拒绝等 11 项判据。它是最小自检入口，不能代表所有渲染功能或性能均已验收。

2026-11-17 在 Windows x64、MinGW-w64 GCC 15.2.0、CMake 4.3.1 下，对此副本进行了以下验证：

| 检查 | 结果 |
|---|---|
| CMake 配置及 Release 构建 | 成功；GCC 保留一处原源码的未使用变量警告：`bvh_node.h` 中的 `root_idx` |
| CTest / 内置 `exam` | 1 个 CTest 测试通过；11 项内置判据通过，`failures=0` |
| Cornell 小图渲染 | 64 × 64、8 spp、depth 10、BVH 开启、2 个线程，退出码 0 |
| 输出检查 | PPM 尺寸及 12288 个颜色通道值有效，计时文件成功生成 |

这些结果仅针对上述工具链与配置。Linux、macOS 和 MSVC 构建入口尚未在本次整理中执行，未重新测量性能或重跑原工程完整历史验证套件。

## 当前限制

- 这是 CPU 学习渲染器，尚未实现显式光源采样 / MIS、交互式预览或去噪；低样本 Cornell 图像会有明显噪声。
- 线程数未设置上限，过大的输入可能造成创建开销或资源失败；尺寸与样本数上限也不等于合理的运行预算。
- 固定种子用于复现同一工具链下的结果，不保证不同标准库与浮点实现逐字节一致。
- `--timing` 目前只有 Cornell 分支实现；`checker` 接受该参数但不会输出计时文件。
- PPM 文件名不包含所有渲染参数。运行不同实验时应使用不同输出目录。
- 未在本次整理中重新证明 BVH 或多线程的性能收益，因此不提供未经复测的提速倍数。
