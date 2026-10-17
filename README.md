# XSSTV

一个使用 C++ 实现的 Robot36 模式 SSTV（慢扫描电视）编解码软件。包含纯 C++ 编写的核心算法库、CLI 工具以及现代化的 Qt 桌面界面。

## 项目展示

**编码**
<p align="center">
  <img src="docs/images/XSSTV_encode.gif" alt="编码展示" width="800">
</p>

**解码** 
<p align="center">
  <img src="docs/images/XSSTV_decode.gif" alt="解码展示" width="800">
</p>

## 功能特性

### 核心编解码（Core）
- **Robot36 编码**：将任意尺寸图片等比例缩放、居中黑边填充后，调制成标准 SSTV 音频（PCM16 WAV）
- **Robot36 解码**：从音频中提取瞬时频率，还原为 320×240 图像

### 信号处理与算法
- **正交解调（I/Q）**：使用正交混频与低通滤波提取基带信号，计算相位差求频率
- **相位解缠绕**：解决 `atan2` 在 ±π 处的跳变，保证音频连续性与解码精度
- **BT.601 色彩空间**：支持 RGB 与 YCrCb 相互转换，并实现 4:2:0 降采样以节省带宽
- **平滑与去噪**：包含滑动平均低通、边缘修复与 3x3 模糊，提升解码画质

### 命令行工具（CLI）
- 提供 `encode` 和 `decode` 子命令，适合脚本调用与自动化测试
- 支持自定义输入输出路径，未指定时自动回退到默认目录
- `--debug` 模式可将中间处理图片（如缩放后的预览图）落盘，方便排查

### 桌面图形界面（Qt 6）
- 支持图片和 WAV 文件拖拽加载、路径记忆
- 内置音频播放器，支持播放 / 暂停 / 停止
- 提供独立的资源管理面板，支持查看、搜索、重命名与清理

### 其他
- 支持 WAV 文件格式自动解析，兼容跳过额外的 RIFF 块并自动将多声道降为单声道

## 技术栈

**语言**
- `C++17`

**构建系统**
- `CMake`
- `Ninja`

**图像与信号处理**
- `stb_image` / `stb_image_write` / `stb_image_resize`
- 纯手写 DSP 算法（正交解调、相位解缠绕）

**桌面 GUI**
- `Qt 6 Widgets`
- `Qt Multimedia`
- `Qt Concurrent`

## 运行方式

- 使用 **MSVC** 编译
- 配置 **Qt 6** 环境（仅桌面端需要；Core / CLI 不需要）
- 编译运行项目

```bat
:: Core + CLI（不需要 Qt）
cmake -S . -B build -G Ninja
cmake --build build

:: 桌面端（需要 Qt）
cmake -S . -B build-desktop -G Ninja ^
      -DXSSTV_BUILD_DESKTOP=ON ^
      -DCMAKE_PREFIX_PATH=<Qt的msvc2022_64目录> ^
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-desktop
```
也可直接到 [Releases](https://github.com/XLAIWASHI/XSSTV/releases) 下载编译好的可执行文件。

## 操作说明

1. **编码**：选择图片 → 开始编码 → 播放 / 保存 WAV
2. **解码**：选择 WAV → 开始解码 → 保存图片
3. **拖拽**：拖入图片自动进编码页；拖入 WAV 自动进解码页
4. **播放**：播放 / 暂停 / 停止
5. **资源管理**：查看、筛选、搜索、打开目录、另存为、重命名、删除