# XSSTV 项目设计方案

**项目名称**：XSSTV
**版本**：V1.0
**项目类型**：跨平台 SSTV 编解码程序
**核心语言**：C++17
**GUI 框架**：Qt 6
**目标平台**：Windows / Android
**第一版 SSTV 模式**：Robot 36

---

# 一、项目目标

XSSTV 是一个使用 C++ 实现的 SSTV（Slow Scan Television，慢扫描电视）编解码程序。

项目的核心目标是完整实现以下过程：

```text
图片
 ↓
Robot 36 编码
 ↓
SSTV 音频
 ↓
WAV / 播放 / 无线电传输
 ↓
SSTV 解码
 ↓
图片
```

最终程序支持：

* Robot 36 编码
* Robot 36 解码
* WAV 文件读写
* 图片读写
* WAV 文件解码
* 麦克风实时接收与解码
* Windows 桌面运行
* Android 运行
* Android 图片保存到系统相册
* 默认图片、音频、输出位置设置

项目完成后，**不以继续增加 SSTV 模式或长期维护为目标**。

---

# 二、项目原则

## 2.1 核心算法与界面分离

SSTV 编码、解码、DSP 等核心功能使用纯 C++ 实现。

Core 不依赖 Qt。

```text
Qt / Android
      ↓
XSSTV Core
```

Qt 负责：

* 用户界面
* 文件选择
* 图片显示
* 音频播放
* 麦克风输入
* 应用设置
* Android 系统功能

---

## 2.2 第一版只实现 Robot 36

第一版不实现：

* PD120
* Martin
* Scottie
* 其他 SSTV 模式
* 多模式自动识别

首先把 Robot 36 完整实现。

---

## 2.3 先文件，后实时

开发顺序：

```text
图片
 ↓
Robot 36 编码
 ↓
WAV
 ↓
Robot 36 解码
 ↓
图片
```

确认文件编解码正确以后，再实现：

```text
麦克风
 ↓
实时音频
 ↓
Robot 36 解码
 ↓
实时图像
```

---

# 三、总体架构

```text
                         XSSTV
                           │
             ┌─────────────┴─────────────┐
             │                           │
        Desktop Qt                  Android Qt
             │                           │
             └─────────────┬─────────────┘
                           │
                     XSSTV Core
                           │
          ┌────────────────┼────────────────┐
          │                │                │
        Image            Audio             DSP
          │                │                │
          └────────────────┼────────────────┘
                           │
                          SSTV
                           │
                        Robot 36
                       ┌────┴────┐
                       │         │
                    Encoder   Decoder
```

核心依赖方向：

```text
Application
     ↓
  Core
```

Core 不反向依赖 Application。

---

# 四、Core 设计

Core 是项目的核心部分，负责：

* 图像数据
* 音频数据
* WAV
* 颜色空间转换
* DSP
* SSTV 编码
* SSTV 解码

主要结构：

```text
core/
├── image/
├── audio/
├── dsp/
└── sstv/
```

---

# 五、Image 模块

Image 模块负责图片数据和颜色空间转换。

## 5.1 RGB

程序使用 RGB 作为通用图片格式：

```cpp
struct Image
{
    int width = 0;
    int height = 0;

    std::vector<uint8_t> rgb;
};
```

主要用于：

* PNG/JPG 读取
* PNG/JPG 保存
* Qt 显示
* 最终图片输出

---

## 5.2 YCrCb

Robot 36 内部使用亮度和色差信息，因此 SSTV 数据处理使用 YCrCb。

```cpp
struct YCrCbImage
{
    int width = 0;
    int height = 0;

    std::vector<uint8_t> y;
    std::vector<uint8_t> cr;
    std::vector<uint8_t> cb;
};
```

其中：

```text
Y  → 亮度
Cr → 色差
Cb → 色差
```

---

## 5.3 颜色转换

编码：

```text
RGB
 ↓
YCrCb
 ↓
Robot 36
```

解码：

```text
Robot 36
 ↓
YCrCb
 ↓
RGB
 ↓
PNG/JPG
```

YCrCb 本身可以表示完整图像，并不是因为它不能作为图像才转换为 RGB。

转换为 RGB 的主要原因是：

* 普通图片文件更适合使用 RGB/RGBA
* Qt 显示方便
* 普通图片查看器兼容性好
* 最终输出更符合用户习惯

---

## 5.4 Image 功能

提供：

```cpp
Image load_image(...);

void save_image(...);

Image resize(
    const Image& image,
    int width,
    int height
);

YCrCbImage rgb_to_ycrcb(
    const Image& image
);

Image ycrcb_to_rgb(
    const YCrCbImage& image
);
```

---

# 六、Audio 模块

音频使用统一的内存格式：

```cpp
struct AudioBuffer
{
    int sample_rate = 48000;

    std::vector<float> mono;
};
```

其中：

```text
单声道
float
范围约为 [-1, 1]
```

第一版主要支持：

```text
48000 Hz
Mono
PCM16 WAV
```

---

# 七、WAV 模块

WAV 使用自实现的 PCM WAV 读写。

```cpp
AudioBuffer read_wav(...);

void write_wav(
    ...,
    const AudioBuffer& audio
);
```

第一版只要求：

```text
PCM
16 bit
Mono
48000 Hz
```

不为其他音频格式增加复杂支持。

---

# 八、音调生成

编码器需要产生 SSTV 使用的正弦波。

提供：

```cpp
void append_tone(
    AudioBuffer& audio,
    double frequency,
    double duration
);
```

以及：

```cpp
void append_silence(
    AudioBuffer& audio,
    double duration
);
```

用于生成：

* 引导信号
* VIS
* 同步信号
* 图像数据

---

# 九、DSP 模块

DSP = Digital Signal Processing，即数字信号处理。

DSP 在 XSSTV 中主要负责：

```text
音频
 ↓
分析频率
 ↓
检测同步
 ↓
提供给 SSTV Decoder
```

主要包括：

```text
Goertzel
频率检测
同步检测
必要的滤波/信号处理
```

---

# 十、Goertzel

第一版使用 Goertzel 检测指定频率的能量。

```cpp
double goertzel_energy(
    const float* samples,
    size_t count,
    double sample_rate,
    double frequency
);
```

用于判断当前音频窗口中某个频率的强度。

例如：

```text
输入一段音频
 ↓
检测候选频率
 ↓
找出能量最大的频率
 ↓
得到当前 SSTV 数据
```

Goertzel 只是 Decoder 使用的一个 DSP 工具，并不等于整个解码过程。

---

# 十一、同步检测

Decoder 需要确定：

```text
SSTV 从哪里开始？
一行从哪里开始？
当前像素应该从哪里采样？
```

因此提供同步检测功能：

```cpp
struct SyncResult
{
    size_t position;
    bool found;
};
```

同步检测与频率检测共同完成音频时序定位。

---

# 十二、Robot 36 模块

Robot 36 是第一版唯一实现的 SSTV 模式。

目录：

```text
core/
└── sstv/
    └── robot36/
        ├── Robot36Timing.h
        ├── Robot36Encoder.cpp
        ├── Robot36Decoder.cpp
        └── Robot36Vis.cpp
```

---

# 十三、Robot 36 Encoder

编码流程：

```text
输入图片
 ↓
缩放到 Robot 36 所需尺寸
 ↓
RGB → YCrCb
 ↓
生成引导信号
 ↓
生成 VIS
 ↓
生成同步信号
 ↓
生成亮度数据
 ↓
生成色差数据
 ↓
AudioBuffer
 ↓
WAV
```

编码器只负责按照 Robot 36 协议生成音频。

具体协议参数以实际 Robot 36 协议资料为准，并通过外部软件进行交叉验证。

---

# 十四、Robot 36 Decoder

解码流程：

```text
WAV
 ↓
AudioBuffer
 ↓
寻找 SSTV 信号
 ↓
检测同步
 ↓
解析 VIS
 ↓
确认 Robot 36
 ↓
逐行解码
 ↓
得到 YCrCb
 ↓
YCrCb → RGB
 ↓
Image
 ↓
PNG/JPG
```

Decoder 的关键不是单纯“检测频率”，而是同时解决：

* 信号起始位置
* 同步位置
* 行位置
* 像素时间
* 频率检测
* 图像重建

---

# 十五、Robot 36 协议参数

Robot 36 的具体协议参数在实现阶段根据可靠协议资料确定。

需要确认：

* 图像尺寸
* VIS
* 同步频率
* 同步时间
* 图像数据频率范围
* 行结构
* 像素时间
* Y 数据传输方式
* Cr/Cb 传输方式

设计文档中的参数不能在未经验证的情况下直接视为最终协议参数。

---

# 十六、文件解码与实时解码

两种输入最终使用同一套 Robot 36 解码逻辑。

文件：

```text
WAV
 ↓
AudioBuffer
 ↓
Decoder
 ↓
Image
```

实时：

```text
麦克风
 ↓
连续音频
 ↓
Decoder
 ↓
Image
```

不重复实现两套 Decoder。

---

# 十七、RealTimeDecoder

实时解码采用状态机。

```text
SEARCH
 ↓
VIS
 ↓
SYNC
 ↓
DECODE_LINE
 ↓
SYNC
 ↓
DECODE_LINE
 ↓
……
```

基本接口：

```cpp
class RealTimeDecoder
{
public:
    void feed(
        const float* samples,
        size_t count
    );

    bool has_new_image() const;

    Image take_image();
};
```

实时系统在后期再加入：

* 音频缓冲
* 独立解码线程
* UI 定时刷新

第一阶段不提前实现复杂实时架构。

---

# 十八、CLI

CLI = Command Line Interface，即命令行界面。

CLI 不作为主要用户界面，而是作为开发和测试工具。

例如：

```bash
xsstv encode input.png output.wav
```

```bash
xsstv decode input.wav output.png
```

作用：

* 快速测试 Encoder
* 快速测试 Decoder
* 不依赖 Qt
* 方便自动化测试

CLI 与 Qt 使用同一个 Core。

---

# 十九、Qt Desktop

桌面程序使用：

```text
Qt 6 Widgets
Qt Designer
Qt Multimedia
```

主要功能：

```text
图片 → Robot 36 → WAV

WAV → Robot 36 → 图片
```

以及：

* 图片选择
* WAV 选择
* WAV 播放
* 图片显示
* 麦克风实时接收
* 解码结果保存
* 默认路径设置

Qt 不实现 SSTV 算法。

---

# 二十、Android

Android 使用 Qt for Android。

Android 与桌面共用：

```text
XSSTV Core
```

Android 只负责平台相关功能：

* 用户界面
* 文件选择
* 麦克风
* 音频
* 图片显示
* 设置
* 系统相册

---

# 二十一、Android 保存到相册

这是正式功能。

流程：

```text
SSTV Decoder
 ↓
RGB Image
 ↓
Android 图片保存接口
 ↓
系统照片 / 图库
```

用户解码完成后，可以直接在手机系统相册中看到结果。

该功能属于 Android Application 层，不放入 Core。

---

# 二十二、默认路径

应用支持修改默认位置。

主要包括：

```text
默认图片目录
默认音频目录
默认输出目录
```

例如桌面：

```text
打开图片
 ↓
默认进入用户设置的图片目录
```

保存 WAV：

```text
保存 WAV
 ↓
默认进入用户设置的音频目录
```

保存图片：

```text
保存图片
 ↓
默认进入用户设置的输出目录
```

Android 使用 Android 自身的文件访问机制，不强行使用 Windows 式路径。

---

# 二十三、设置模块

设置属于 Application 层。

```text
Settings
├── 默认图片位置
├── 默认音频位置
└── 默认输出位置
```

Core 不保存用户路径，也不依赖平台文件系统。

---

# 二十四、项目目录

```text
XSSTV/
│
├── CMakeLists.txt
├── README.md
├── .gitignore
│
├── core/
│   ├── CMakeLists.txt
│   │
│   ├── include/
│   │   └── xsstv/
│   │       ├── Image.h
│   │       ├── AudioBuffer.h
│   │       ├── Color.h
│   │       ├── Wav.h
│   │       └── Robot36.h
│   │
│   └── src/
│       ├── image/
│       │   ├── image_io.cpp
│       │   └── color.cpp
│       │
│       ├── audio/
│       │   ├── wav.cpp
│       │   └── tone.cpp
│       │
│       ├── dsp/
│       │   ├── goertzel.cpp
│       │   ├── frequency.cpp
│       │   └── sync.cpp
│       │
│       └── sstv/
│           └── robot36/
│               ├── Robot36Timing.h
│               ├── Robot36Encoder.cpp
│               ├── Robot36Decoder.cpp
│               └── Robot36Vis.cpp
│
├── cli/
│   ├── CMakeLists.txt
│   └── main.cpp
│
├── tests/
│   ├── CMakeLists.txt
│   ├── test_wav.cpp
│   ├── test_color.cpp
│   ├── test_goertzel.cpp
│   └── test_robot36.cpp
│
├── desktop/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── MainWindow.h
│   ├── MainWindow.cpp
│   └── MainWindow.ui
│
├── android/
│   └── ...
│
├── third_party/
│   └── stb/
│
└── assets/
    └── test.png
```

---

# 二十五、依赖关系

保持单向依赖：

```text
Desktop Qt ──┐
             ├──→ XSSTV Core
Android Qt ──┤
             │
CLI ─────────┘
```

Core 不依赖：

```text
Qt
Android
Windows API
GUI
```

---

# 二十六、测试计划

## 26.1 WAV 测试

```text
AudioBuffer
 ↓
WAV
 ↓
AudioBuffer
```

确认数据能够正确读写。

---

## 26.2 DSP 测试

使用已知频率的测试信号：

```text
1500 Hz
1900 Hz
2300 Hz
```

验证频率检测。

---

## 26.3 颜色转换测试

```text
RGB
 ↓
YCrCb
 ↓
RGB
```

检查转换误差。

---

## 26.4 闭环测试

最重要的测试：

```text
原始图片
 ↓
Robot 36 Encoder
 ↓
WAV
 ↓
Robot 36 Decoder
 ↓
恢复图片
```

比较：

* 图像尺寸
* 颜色
* 亮度
* 整体图像内容

可以使用 PSNR / SSIM 等指标辅助判断。

---

## 26.5 外部兼容测试

使用其他 SSTV 软件进行：

```text
其他软件
 ↓
Robot 36 WAV
 ↓
XSSTV Decoder
```

以及：

```text
XSSTV Encoder
 ↓
Robot 36 WAV
 ↓
其他 SSTV 软件
```

确保 XSSTV 不只是“自己编码、自己解码”。

---

## 26.6 真实信号测试

使用真实 SSTV 录音：

```text
真实 SSTV 信号
 ↓
WAV
 ↓
XSSTV Decoder
 ↓
图片
```

用于验证实际信号环境下的解码能力。

---

## 26.7 实时测试

```text
SSTV 音频
 ↓
扬声器 / 音频设备
 ↓
麦克风
 ↓
XSSTV
 ↓
实时图像
```

确认实时接收链路正常。

---

# 二十七、开发阶段

## Phase 0：项目骨架

建立：

* Git
* CMake
* Core
* CLI
* Tests
* Desktop

---

## Phase 1：WAV

完成：

```text
read_wav
write_wav
```

---

## Phase 2：音频生成

完成：

```text
正弦波
静音
AudioBuffer
```

---

## Phase 3：DSP

完成：

```text
Goertzel
频率检测
同步检测
```

---

## Phase 4：图像

完成：

```text
PNG/JPG
RGB
YCrCb
resize
颜色转换
```

---

## Phase 5：Robot 36 Encoder

完成：

```text
Image → Robot 36 → AudioBuffer
```

然后：

```text
AudioBuffer → WAV
```

---

## Phase 6：Robot 36 Decoder

完成：

```text
WAV → AudioBuffer → Robot 36 → Image
```

---

## Phase 7：闭环

完成：

```text
Image
 ↓
Encoder
 ↓
WAV
 ↓
Decoder
 ↓
Image
```

这一阶段是整个项目的第一个关键里程碑。

---

## Phase 8：外部兼容

测试：

```text
XSSTV ↔ 其他 SSTV 软件
```

---

## Phase 9：CLI

完善：

```text
encode
decode
```

用于方便测试。

---

## Phase 10：Qt Desktop

实现：

* 图片编码
* WAV 解码
* WAV 播放
* 图片显示
* 文件保存

---

## Phase 11：实时解码

实现：

```text
麦克风
 ↓
实时 Decoder
 ↓
实时图像
```

---

## Phase 12：Android

完成：

* Android GUI
* 文件选择
* 音频输入
* 编解码
* 图片显示

---

## Phase 13：系统功能

完成：

* 保存图片到 Android 相册
* 默认路径
* 输出路径
* 应用设置

---

# 二十八、第一版明确不做

为了控制项目范围，以下内容不属于 V1.0：

```text
PD120
Martin
Scottie
多模式自动识别
复杂图像编辑
SDR
无线电 CAT 控制
网络 SSTV
高级抗噪
高级均衡
插件系统
云服务
复杂 SDK
```

如果未来产生需求，再作为独立版本考虑。

---

# 二十九、项目完成标准

XSSTV V1.0 满足以下条件即可认为完成：

### 核心

```text
Robot 36 编码正常
Robot 36 解码正常
WAV 正常读写
图片正常读写
```

### 闭环

```text
图片
 ↓
Robot 36
 ↓
WAV
 ↓
Robot 36
 ↓
图片
```

能够正常完成。

### 外部兼容

能够与其他 Robot 36 SSTV 软件进行基本交叉验证。

### 实时

```text
麦克风
 ↓
Robot 36
 ↓
实时图像
```

能够正常工作。

### Windows

能够正常使用 Qt 桌面程序。

### Android

能够正常：

```text
选择图片
编码
解码
实时接收
保存图片
```

并能够将解码图片保存到系统相册。

### 设置

能够修改默认图片、音频和输出位置。

满足以上条件后：

> **XSSTV V1.0 即视为项目完成。**

不再为了未来可能出现的需求继续扩大项目范围。

---

# 三十、核心技术路线总结

XSSTV 的核心技术路线只有一条：

```text
                 ┌─────────────┐
                 │  RGB Image  │
                 └──────┬──────┘
                        │
                   RGB → YCrCb
                        │
                        ▼
                 ┌─────────────┐
                 │ Robot 36    │
                 │   Encoder  │
                 └──────┬──────┘
                        │
                        ▼
                    AudioBuffer
                        │
                        ▼
                       WAV
                        │
              文件 / 播放 / 无线电
                        │
                        ▼
                    AudioBuffer
                        │
                        ▼
                 ┌─────────────┐
                 │ Robot 36    │
                 │   Decoder  │
                 └──────┬──────┘
                        │
                        ▼
                      YCrCb
                        │
                   YCrCb → RGB
                        │
                        ▼
                 ┌─────────────┐
                 │  RGB Image  │
                 └──────┬──────┘
                        │
              ┌─────────┴─────────┐
              │                   │
             PNG/JPG            Qt显示
              │
              ▼
         Android 相册
```

最终开发原则：

> **先让协议跑通，再让程序好用；先完成核心闭环，再做实时和跨平台；不为尚未存在的需求提前增加复杂度。**
