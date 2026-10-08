# XSSTV 项目开发方案 V0.2

## 一、项目目标

使用纯 C++ 实现一个跨平台 SSTV 编解码核心。

第一版只实现 **Robot36**，后续如果有需要再增加其他 SSTV Mode。

最终目标：

```text
图片 → SSTV 音频        编码
SSTV 音频 → 图片        解码

支持：
├── WAV 文件
├── 麦克风实时解码
├── Windows / Linux
└── Android
```

Qt 只负责界面、文件选择、音频设备、图片显示等平台相关功能。

---

# 二、整体架构

```text
                         XSSTV
                           │
              ┌────────────┴────────────┐
              ↓                         ↓
            CLI                    Desktop / Android
         命令行程序                    应用程序
              │                         │
              └────────────┬────────────┘
                           ↓
                         Core
                           │
             ┌─────────────┼─────────────┐
             ↓             ↓             ↓
           Image         Audio          SSTV
                                         │
                                      Robot36
```

核心原则：

```text
Core
    只负责核心数据处理和算法
    不依赖 Qt
    不负责用户界面
    不负责程序入口

CLI / Desktop / Android
    负责与用户交互
    调用 Core
    不重复实现 SSTV 算法
```

可以把 Core 理解为“发动机”，CLI / Desktop / Android 是不同的“外壳”。

---

# 三、Core 内部结构

```text
core/
├── include/xsstv/       ← 对外公开接口
└── src/                 ← Core 内部实现
```

### `include/xsstv/`

放外部程序真正需要使用的类型和函数。

例如：

```text
Image.h
AudioBuffer.h
Wav.h
Robot36.h
```

CLI / Qt / Android 可以：

```cpp
#include "xsstv/Robot36.h"
```

但不需要知道 Robot36 内部是怎么实现的。

---

### `src/`

放具体实现：

```text
src/
├── image/
├── audio/
├── dsp/
└── sstv/
    └── robot36/
```

内部常量、辅助函数、实现细节尽量留在这里。

---

# 四、功能模块

```text
core/src/
│
├── image/
│   ├── image_io.cpp
│   ├── resize.cpp
│   └── color.cpp
│
├── audio/
│   ├── wav.cpp
│   └── tone.cpp
│
├── dsp/
│   ├── goertzel.cpp
│   └── sync.cpp
│
└── sstv/
    └── robot36/
        ├── Robot36.cpp
        └── Robot36Timing.h
```

模块职责：

### image

处理图片本身：

```text
读取
保存
缩放
颜色转换
```

### audio

处理音频数据：

```text
WAV
正弦波
AudioBuffer
```

### dsp

主要服务于解码：

```text
频率检测
同步检测
信号分析
```

### sstv/robot36

实现 Robot36 协议：

```text
VIS
Timing
扫描
编码
解码
```

依赖方向：

```text
Robot36
   ↓
image / audio / dsp

image / audio / dsp
   ↑
不依赖 Robot36
```

即：

> 通用模块不知道 SSTV 的存在，Robot36 使用通用模块。

---

# 五、Robot36 的类设计

目前采用一个公开的 `Robot36` 类：

```cpp
class Robot36
{
public:
    explicit Robot36(int sample_rate = 48000);

    AudioBuffer encode(const Image& image);
    Image decode(const AudioBuffer& audio);

private:
    // 内部实现
};
```

它代表：

> 一个 Robot36 编解码器。

而不是：

```text
Robot36
    ↓
Robot36Encoder
    ↓
EncoderEncoder...
```

暂时不进行多层封装。

如果以后 Encoder / Decoder 内部非常复杂，再进行拆分。

---

# 六、`.h` 和 `.cpp` 的规则

不要机械地认为：

```text
每个 cpp 必须对应一个 h
```

真正的原则是：

```text
需要被其他文件使用的声明
        ↓
放到 .h

具体实现
        ↓
放到 .cpp
```

例如公开接口：

```text
include/xsstv/Robot36.h
        ↓
class Robot36
```

实现：

```text
src/sstv/robot36/Robot36.cpp
```

而：

```text
Robot36Timing.h
```

如果只给 Robot36 内部使用，就留在 `src`。

如果某个辅助函数完全是 `.cpp` 内部实现，也可以直接放在 `.cpp` 中，不需要额外 `.h`。

---

# 七、数据流

## 编码

```text
Image
  ↓
resize
  ↓
RGB → Y / 色差信息
  ↓
Robot36
  ↓
AudioBuffer
  ↓
WAV
```

## 解码

```text
WAV / 麦克风
  ↓
AudioBuffer
  ↓
DSP
  ↓
Robot36 Decoder
  ↓
图像数据
  ↓
Image
  ↓
PNG
```

---

# 八、Image 的第一版设计

先使用简单的 RGB888：

```cpp
struct Image
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgb;
};
```

其中：

```text
rgb =

R G B R G B R G B ...
```

每个像素 3 字节。

例如：

```text
320 × 240 × 3
=
230400 bytes
```

暂时不加入：

```text
alpha
stride
多种像素格式
复杂图像类层次
```

等真正需要时再扩展。

---

# 九、Robot36 协议参数

`Robot36Timing.h` 保存 Robot36 的内部协议常量。

例如：

```cpp
constexpr double FREQ_SYNC = 1200.0;
constexpr double FREQ_MIN  = 1500.0;
constexpr double FREQ_MAX  = 2300.0;

constexpr double TIME_SYNC        = 9.0;
constexpr double TIME_SYNC_PORCH  = 3.0;
constexpr double TIME_Y_SCAN      = 88.0;
```

但是有一个原则：

> **不要把网上看到的所有数字直接当成“已经确认的标准”。**

特别是：

```text
VIS 时序
色差排列
行结构
采样方式
```

正式实现之前逐项根据可靠资料确认。

---

# 十、不要过早固定内部图像结构

目前只确定：

```text
输入
RGB Image
```

然后由 Robot36 编码过程中完成所需的颜色转换和采样。

暂时不要把：

```text
Y  = 320×240
Cr = 160×240
Cb = 160×240
```

直接定义成整个 Core 的固定图像结构。

这些属于 Robot36 Encoder 的实现细节。

---

# 十一、开发方式：从小功能开始

不要直接写：

```text
Robot36Encoder.cpp
```

然后试图一次完成整个 SSTV。

采用：

```text
一个功能
    ↓
实现
    ↓
编译
    ↓
测试
    ↓
理解
    ↓
下一功能
```

---

# 十二、开发阶段

## 第一阶段：Image IO

目标：

```text
PNG
 ↓
Image
 ↓
PNG
```

实现：

```text
Image.h
image_io.cpp
CLI 测试
```

验证：

```text
读取 test.png
 ↓
打印 width / height
 ↓
保存 copy.png
 ↓
确认图片正确
```

---

## 第二阶段：Resize

实现：

```text
resize.cpp
```

目标：

```text
任意图片
 ↓
320×240
```

先验证缩放结果，再进入 SSTV。

---

## 第三阶段：颜色转换

实现：

```text
color.cpp
```

理解并实现：

```text
RGB
 ↓
Robot36 所需要的亮度 / 色差信息
```

这一阶段重点是理解颜色空间，而不是追求复杂优化。

---

## 第四阶段：Audio 基础

实现：

```text
AudioBuffer
tone.cpp
wav.cpp
```

首先解决：

```text
生成 1900 Hz 正弦波
 ↓
保存 WAV
 ↓
播放器 / Audacity 检查
```

然后实现不同频率的音调。

---

## 第五阶段：Robot36 VIS

这是第一次真正进入 SSTV 协议。

目标：

```text
生成 Robot36 VIS
 ↓
AudioBuffer
 ↓
WAV
```

验证：

```text
Leader
Break
VIS
Parity
Stop
```

逐段检查频率和时间。

不要一开始就生成整张图片。

---

## 第六阶段：Robot36 单行

实现：

```text
同步
 ↓
Y 扫描
 ↓
分隔
 ↓
色差信息
```

目标：

> 先能够正确生成一行 Robot36 信号。

这一阶段重点理解：

```text
图像数据
      ↓
数值
      ↓
频率
      ↓
随时间变化的音频
```

---

## 第七阶段：完整编码

```text
Image
 ↓
Resize
 ↓
颜色转换
 ↓
VIS
 ↓
240 行
 ↓
AudioBuffer
 ↓
WAV
```

最终实现：

```text
图片 → Robot36 WAV
```

---

## 第八阶段：编码闭环

使用自己的编码器：

```text
test.png
 ↓
Robot36 Encoder
 ↓
output.wav
 ↓
Robot36 Decoder
 ↓
decoded.png
```

目标：

> 自己编码的音频能够被自己的 Decoder 正确恢复。

---

## 第九阶段：解码真实 SSTV

加入：

```text
Goertzel
同步检测
频率检测
VIS 识别
```

然后测试真实 Robot36 音频。

---

## 第十阶段：CLI 完善

最终 CLI 可以提供：

```text
xsstv encode input.png output.wav
xsstv decode input.wav output.png
```

CLI 只负责：

```text
参数
文件
错误信息
调用 Core
```

不负责 SSTV 算法。

---

## 第十一阶段：Qt Desktop

Qt 负责：

```text
选择图片
选择 WAV
播放音频
显示图片
文件保存
```

核心算法继续使用 Core。

---

## 第十二阶段：实时麦克风

加入平台音频输入：

```text
麦克风
 ↓
AudioBuffer
 ↓
DSP
 ↓
Robot36 Decoder
 ↓
Image
```

---

## 第十三阶段：Android

Android 只提供新的外壳和平台能力：

```text
Android UI
 ↓
Core
 ↓
Robot36
```

Core 不需要重新实现。

---

# 十三、当前真正要做的事情

现在不要继续设计整个 XSSTV。

当前任务只有：

```text
core/include/xsstv/Image.h
        ↓
core/src/image/image_io.cpp
        ↓
stb_image
        ↓
CLI
        ↓
PNG → Image → PNG
```

成功之后再进入 Resize。

---

# 十四、最终原则

### 原则 1：先理解，再实现

特别是 Robot36：

```text
原理
 ↓
协议
 ↓
数据结构
 ↓
算法
 ↓
代码
```

---

### 原则 2：每一步都能验证

不要：

```text
写 1000 行
 ↓
最后发现不知道哪里错
```

而是：

```text
写一点
 ↓
测试
 ↓
确认
 ↓
继续
```

---

### 原则 3：不要为了未来过度设计

现在只做 Robot36。

以后如果真的需要：

```text
Martin
Scottie
其他 Mode
```

再在：

```text
src/sstv/
```

下面增加对应实现。

---

### 原则 4：Core 与应用分离

```text
Core：
“怎么做 SSTV？”

CLI / Qt / Android：
“用户要做什么？”
```

两者不要混在一起。

---

# 十五、最终项目关系

```text
                         XSSTV
                           │
              ┌────────────┴────────────┐
              │                         │
             CLI                    Desktop / Android
              │                         │
              └────────────┬────────────┘
                           ↓
                         Core
                           │
       ┌───────────────────┼───────────────────┐
       ↓                   ↓                   ↓
     Image               Audio               SSTV
       │                   │                   │
   IO/Resize/Color    WAV/Tone             Robot36
                                           │
                                      Encode/Decode
```

核心目标不是一次把整个系统写完，而是：

```text
Image IO
   ↓
Resize
   ↓
Color
   ↓
Audio
   ↓
Tone
   ↓
VIS
   ↓
单行
   ↓
完整 Robot36
   ↓
Decoder
   ↓
CLI
   ↓
Qt
   ↓
Android
```

**每完成一层，就得到一个可以运行、可以验证、可以理解的结果。**
