# XSSTV

用 C++17 实现的 **Robot 36 模式 SSTV（慢扫描电视）编解码程序**。

把一张图片编码成 SSTV 音频（WAV），或把 SSTV 音频解码回图片。Core 为纯 C++，不依赖 Qt；提供命令行工具和 Qt 桌面界面两种外壳。

```
图片 ──Robot36 编码──▶ SSTV 音频(WAV)
SSTV 音频(WAV) ──Robot36 解码──▶ 图片
```

---

## 功能

- **Robot 36 编码**：图片 → 音频
- **Robot 36 解码**：音频 → 图片
- **图片读写**：PNG / JPG / JPEG / BMP（经 stb）
- **WAV 读写**：16bit PCM；读取时自动多声道降单声道、跳过额外 RIFF 块
- **音频播放**：播放 / 暂停 / 停止
- **桌面 GUI**：编码页、解码页、资源管理（Qt 6 Widgets）
- **其它**：拖拽加载、默认路径记忆、输出自动编号、支持中文路径

---

## 目录结构

```
XSSTV/
├── core/                 纯 C++ 核心（不依赖 Qt）
│   ├── include/xsstv/    公开接口
│   └── src/
│       ├── image/        图片读写 / 缩放 / 颜色转换
│       ├── audio/        WAV 读写 / 正弦波
│       └── sstv/robot36/ Robot36 编解码
├── cli/                  命令行工具
├── desktop/              Qt 桌面程序
├── third_party/stb/      单头文件图像库
├── docs/                 文档
└── assets/               素材（输入图片）
```

依赖方向：`cli / desktop ──▶ core`，**core 不反向依赖**。

---

## 环境要求

- **C++17**，CMake ≥ 3.16
- **编译器**：MSVC（推荐 VS 2022 / 2019 的 MSVC 工具链）
- **Qt 6**（仅桌面端需要）：Widgets、Multimedia、Concurrent
- 目标平台：Windows（桌面）

---

## 构建

### 1. Core + CLI（不需要 Qt）

```bat
cmake -S . -B build -G Ninja
cmake --build build
```
产物：`build/core/xsstv_core.lib`、`build/cli/xsstv.exe`

### 2. 桌面端（需要 Qt）

```bat
cmake -S . -B build-desktop -G Ninja ^
      -DXSSTV_BUILD_DESKTOP=ON ^
      -DCMAKE_PREFIX_PATH=<Qt的msvc2022_64目录> ^
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-desktop
```
> 用 CMake 时需在 **VS 开发者命令行**里（`cl.exe`/`ninja` 可用），并加载 vcvars 环境。
> `<Qt的msvc2022_64目录>` 例如 `D:/Qt/6.11.2/msvc2022_64`。

产物：`build-desktop/desktop/xsstv_desktop.exe`

---

## 使用

### 命令行

```
xsstv encode [--debug] [输入图片] [输出WAV]
xsstv decode [--debug] [输入WAV] [输出PNG]
```
- 输入省略时默认 `assets/test.png`；找不到会在 `assets/` 里找
- 输出省略时自动编号：`output/robot36_NNNN.wav` / `output/decoded_NNNN.png`
- `--debug` 输出中间图到 `temp/`

### 桌面端

双击 `xsstv_desktop.exe`，或直接把图片 / WAV **拖进窗口**。

- **编码页**：选图片 →（左侧原图 / 右侧 320×240 编码输入图）→ 开始编码 → 播放 / 保存 WAV
- **解码页**：选 WAV → 查看音频信息 → 开始解码 → 保存图片
- **资源管理**：查看、筛选、搜索、打开目录、另存为、重命名、删除、预览/播放

---

## 打包分发（桌面端）

```powershell
# 1. 复制 exe 到干净目录
New-Item -ItemType Directory dist
Copy-Item build-desktop\desktop\xsstv_desktop.exe dist\

# 2. 收集 Qt 依赖（DLL + 插件），在 VS 开发者环境里执行
windeployqt --release dist\xsstv_desktop.exe

# 3. 补 MSVC 运行库（从 VS 的 Redist 目录）
Copy-Item "<VS>\VC\Redist\MSVC\<ver>\x64\Microsoft.VC14x.CRT\*.dll" dist\

# 4. 放默认样例图
New-Item -ItemType Directory dist\assets
Copy-Item assets\test.png dist\assets\

# 5. 压缩
Compress-Archive -Path dist\* -DestinationPath xsstv_desktop.zip -Force
```

分发给他人：发 `xsstv_desktop.zip`，对方解压后双击 `xsstv_desktop.exe` 即可（已含 Qt 与运行库）。

---

## 文档

- `docs/Core_Summary.md` —— Core 结构、公开 API、数据流
- `docs/Encoder_Summary.md` —— 编码器实现细节
- `docs/Decoder_Summary.md` —— 解码器实现细节
- `plan.md` / `qt_plan.md` —— 开发计划

---

## 已知限制

- **只支持 Robot 36**（PD120、Martin、Scottie 等尚未实现）
- **不做实时麦克风解码**（当前为文件级编解码）
- 真实无线电录音（噪声 / 多普勒频移 / 时钟差）**未做鲁棒处理**，无斜切校正
- WAV 仅处理 **16bit PCM**；不支持 8/24/32bit、浮点或压缩格式（如 m4a）
- 桌面端仅支持 64 位 Windows（受 Qt 6 msvc2022 限制）
