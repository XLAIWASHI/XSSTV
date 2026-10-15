# XSSTV Core 总结

> 本文档描述 `core/` 的**实际结构**：分层、目录、公开 API、各模块职责、编解码流程。
> 只讲 Core，不含 CLI / Qt（它们是外壳）。

---

## 一、总体架构

### 分层

```text
CLI / Qt / Android        （外壳：界面、文件、参数、平台功能）
        │  只能 include public 头
        ▼
    XSSTV Core            （纯 C++，不依赖 Qt / 平台 / GUI）
```

核心原则：

- **Core 只负责算法与数据**：图像、音频、颜色、编解码。
- **Core 不依赖外壳**；外壳单向依赖 Core。
- **公开接口与内部实现分离**：
  - `core/include/xsstv/` = 对外公开的类型和函数（外壳能用的）
  - `core/src/` = 内部实现（外壳看不到）

### 数据流

编码：

```text
Image(RGB) → 缩放 → RGB→YCrCb(4:2:0) → Robot36 编码 → AudioBuffer → WAV
```

解码：

```text
WAV → AudioBuffer → Robot36 解码(正交解调→同步→采样) → YCrCb → Image(RGB)
```

---

## 二、目录结构

```text
core/
├── CMakeLists.txt
├── include/
│   └── xsstv/                  ← 公开接口（外壳可见）
│       ├── Image.h             Image 结构 + 读写/缩放
│       ├── Color.h             YCrCbImage 结构 + 颜色转换
│       ├── AudioBuffer.h       AudioBuffer 结构
│       ├── Wav.h               WAV 读写
│       └── Robot36.h           Robot36 编解码类
└── src/                        ← 内部实现（外壳不可见）
    ├── image/
    │   ├── image_io.cpp        图片读写（stb_image / stb_image_write）
    │   ├── image_resize.cpp    缩放（stb_image_resize2）
    │   └── color.cpp           RGB ↔ YCrCb
    ├── audio/
    │   ├── wav.cpp             WAV 读写
    │   └── tone.cpp            正弦波生成（当前未使用，见下）
    └── sstv/
        └── robot36/
            ├── Robot36Timing.h 协议常量
            └── Robot36.cpp     Robot36 编/解码实现
```

依赖方向：

```text
Robot36  ──▶  image / audio
image / audio  ──▶  （不依赖 Robot36）
```

即：**通用模块不知道 SSTV 的存在，Robot36 使用通用模块。**

---

## 三、公开 API（`include/xsstv/`）

外壳（CLI/Qt）只需要这些。所有类型/函数都在 `namespace xsstv` 下。

### Image.h

```cpp
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgb;   // RGB888，每像素 3 字节
};

bool  load_image(const std::string& path, Image& out);   // 读图片（PNG/JPG/BMP…）→ RGB
bool  save_image(const std::string& path, const Image& img); // 写 PNG
Image resize_image(const Image& img, int dst_w, int dst_h);  // 拉伸到指定尺寸
Image fit_image(const Image& img, int dst_w, int dst_h);     // 保持宽高比 + 黑边
```

### Color.h

```cpp
struct YCrCbImage {              // 4:2:0
    int width = 0, height = 0;
    std::vector<uint8_t> y;      // width * height
    std::vector<uint8_t> cr;     // (width/2) * (height/2)
    std::vector<uint8_t> cb;     // (width/2) * (height/2)
};

YCrCbImage rgb_to_ycrcb(const Image& img);      // RGB → 亮度/色差
Image      ycrcb_to_rgb(const YCrCbImage& ycc); // 亮度/色差 → RGB
```

### AudioBuffer.h

```cpp
struct AudioBuffer {
    int sample_rate = 48000;              // 采样率
    std::vector<float> mono;              // 单声道，范围约 [-1, 1]
};
```

### Wav.h

```cpp
bool write_wav(const std::string& path, const AudioBuffer& audio); // 写 16bit 单声道 PCM WAV
bool read_wav (const std::string& path, AudioBuffer& out);         // 读 WAV（多声道→单声道）
```

### Robot36.h

```cpp
class Robot36 {
public:
    explicit Robot36(int sample_rate = 48000);
    AudioBuffer encode(const Image& image);        // 图片 → Robot36 音频
    Image       decode(const AudioBuffer& audio);  // Robot36 音频 → 图片
};
```

---

## 四、图像模块（`src/image/`）

### image_io.cpp —— 读写图片

- 用 **stb_image / stb_image_write**（单头文件库）。
- 实现宏 `STB_IMAGE_IMPLEMENTATION` / `STB_IMAGE_WRITE_IMPLEMENTATION` **在这个 .cpp 里定义一次**。
- `load_image`：`stbi_load(path, &w, &h, &c, 3)`（强制 3 通道）→ 拷进 `std::vector` → `stbi_image_free`。
- `save_image`：`stbi_write_png(path, w, h, 3, data, w*3)`。
- 读取支持多种格式（按内容识别），**写出只支持 PNG**。

### image_resize.cpp —— 缩放

- 用 **stb_image_resize2**（`stbir_resize_uint8_linear`，`STBIR_RGB`）。
- `resize_image`：**拉伸**到指定尺寸。
- `fit_image`：**保持宽高比**缩放，多余部分填**黑边**（居中）。

### color.cpp —— 颜色转换

正向 RGB → YCrCb（全范围 BT.601）：

```text
Y  = 0.299 R + 0.587 G + 0.114 B
Cr = 0.500 R - 0.418688 G - 0.081312 B + 128
Cb = -0.168736 R - 0.331264 G + 0.500 B + 128
```

逆向 YCrCb → RGB：

```text
R = Y + 1.402 (Cr-128)
G = Y - 0.344136 (Cb-128) - 0.714136 (Cr-128)
B = Y + 1.772 (Cb-128)
```

- 色差按 **4:2:0** 降采样：取 **2×2 块平均** → `cr/cb` 为 `(w/2)*(h/2)`。
- 全程 `double` 计算，最后钳位到 `0~255`。

---

## 五、音频模块（`src/audio/`）

### wav.cpp —— WAV 读写

`write_wav`：

- 写标准 44 字节头 + **PCM16 单声道** 数据，**小端**。
- `float → int16`：`int16(s * 32767)`；用 `write_u16/write_u32` 逐字节写。
- 头字段：`RIFF` / `WAVE` / `fmt ` / `data`。

`read_wav`：

- **按 RIFF 规则遍历子块**找 `fmt ` 和 `data`（不假设固定偏移，能跳过 `LIST` 等额外块）。
- 读 `channels`，**多声道求平均降为单声道**（`channels==1` 时等于原样）。
- 小端读取，`int16 → float`：`v / 32767.0f`。
- 辅助：`read_tag / read_u16 / read_u32`。

### tone.cpp —— 正弦波生成

- `void append_tone(AudioBuffer&, double freq, double duration_ms)`：生成一段正弦波往末尾追加。
- **注意**：该函数每次调用相位从 0 开始（会造成拼接爆音）。
  Robot36 编码器**并未使用它**，而是用自己内部相位连续的 `append_tone_`（见下）。
  此文件目前保留但**未被调用**。

---

## 六、Robot36 模块（`src/sstv/robot36/`）

### Robot36Timing.h —— 协议常量

```text
频率(Hz):  FREQ_SYNC=1200  FREQ_MIN=1500  FREQ_MAX=2300  FREQ_CENTER=1900
           FREQ_VIS_1=1100  FREQ_VIS_0=1300
时间(ms):  TIME_SYNC=9  TIME_SYNC_PORCH=3  TIME_Y_SCAN=88
           TIME_SEPARATOR=4.5  TIME_CHROMA_PORCH=1.5  TIME_CHROMA_SCAN=44
           TIME_LINE=150（自动求和）
VIS(ms):   TIME_LEADER=300  TIME_BREAK=10  TIME_VIS_START=30
           TIME_VIS_BIT=30  TIME_VIS_STOP=30
图像:      WIDTH=320  HEIGHT=240  PIXEL_TIME=0.275  VIS_CODE=8
```

### Robot36.cpp —— 函数分类

**编码相关：**

| 函数 | 作用 |
|------|------|
| `append_tone_(freq, duration_ms)` | 生成一段正弦波，**相位用成员 `phase_` 累加**（拼接不爆音） |
| `encode_vis()` | 生成引导音 + VIS（含 7 数据位 + 偶校验 + stop） |
| `encode_line(ycc, line)` | 生成一行：同步 + porch + Y 扫描 + 分隔 + porch + 色差扫描 |
| `encode(image)` | 总流程：`fit_image` → `rgb_to_ycrcb` → VIS → 240 行 |

**解码相关：**

| 函数 | 作用 |
|------|------|
| `demodulate_baseband(x, fs, fc, I, Q)` | 正交混频 + 两级滑动平均低通 → 基带 I/Q |
| `instantaneous_frequency(I, Q, fs, fc)` | `atan2` 求相位 + 解缠绕 + 差分 → 逐采样频率 |
| `find_line_syncs(freq, fs)` | 找出每行的 1200Hz 同步脉冲位置（用于定位每行） |
| `freq_to_value(f)` | 频率 → 0~255（`(f-1500)/800*255`，钳位） |
| `avg_freq(freq, a, b)` | 取某像素覆盖范围内频率的平均 |
| `moving_average(x, L)` | 滑动平均（低通用） |
| `blur3x3(in, w, h)` | 色差平面 3×3 平滑 |
| `fix_edges(p, w, h, n)` | 把最外 n 列/行用内侧值覆盖（去边缘彩色） |
| `decode(audio)` | 总流程：测频 → 找同步 → 逐行采样 → 组装 → 转 RGB |

---

## 七、编码流程

```text
Image(RGB)
  ↓ fit_image        —— 等比例缩放到 320×240（多余部分黑边）
  ↓ rgb_to_ycrcb     —— RGB → YCrCb(4:2:0)：Y 320×240，Cr/Cb 160×120
  ↓ encode_vis       —— 引导音 + VIS
  ↓ 240 × encode_line—— 每行 150ms
AudioBuffer → write_wav → WAV
```

**一行（150ms）的结构：**

| 段 | 频率 | 时长 |
|----|------|------|
| 同步 | 1200 | 9ms |
| 同步 porch | 1500 | 3ms |
| Y 扫描 | 1500→2300（320 像素） | 88ms |
| 分隔 | **偶行 1500 / 奇行 2300** | 4.5ms |
| 色差 porch | 1900 | 1.5ms |
| 色差扫描 | 1500→2300（160 样本） | 44ms |

- 每像素/样本：数值 `v` → 频率 `1500 + v/255*800`。
- 色差：**偶数行传 Cr、奇数行传 Cb**，相邻两行共享一行（存 `line/2`）。
- Y/色差扫描用**浮点位置累加**，保证时序精确不漂移。

---

## 八、解码流程

```text
AudioBuffer (48k 单声道)
  ↓ demodulate_baseband   —— 混频 + 低通 → 基带 I/Q
  ↓ instantaneous_frequency—— 相位解缠 + 差分 → 逐采样频率 freq[]
  ↓ find_line_syncs       —— 找每行同步脉冲位置（找不到则退回固定时序）
  ↓ 逐行采样（240 行）
       Y 像素 i：取该像素覆盖范围的频率平均 → freq_to_value
       色差样本 j：偶行 Cr / 奇行 Cb，存 line/2 行
  ↓ 组装 YCrCbImage
  ↓ fix_edges + blur3x3   —— 修边缘 + 平滑色差
  ↓ ycrcb_to_rgb
Image(RGB) → save_image → PNG
```

**关键点：**

- 频率 → 数值：`(f - 1500) / 800 * 255`。
- **同步定位**（`find_line_syncs`）让解码不依赖"音频从 VIS 头开始"，可容忍开头杂音、图像在中间等。
- 正交解调的**镜像分量**（约 3400~4200Hz）需靠低通滤除；滤不干净会出现横条纹/偏色。
- `fix_edges`：扫描段首尾会被相邻音（sync/porch）污染，用内侧值覆盖最外圈。

---

## 九、构建与依赖

- **C++17**，CMake（`cmake_minimum_required 3.16`）。
- `core/CMakeLists.txt`：
  - `add_library(xsstv_core STATIC ...)` 列出所有 `.cpp`。
  - `target_include_directories(... PUBLIC include)`：对外公开头路径。
  - `target_include_directories(... PRIVATE src, third_party/stb)`：仅内部使用。
- 第三方：`third_party/stb/`（stb_image、stb_image_write、stb_image_resize2）。
- 编码/解码/读写的时序都用 `sample_rate`（`ms = fs/1000`），**理论上不锁定采样率**；实际测试用 48k。

---

## 十、已知限制

- **只支持 Robot36**（PD120/Martin 等其它模式需要另写协议）。
- 只保证解**自己编码**或**其它软件生成的干净 Robot36**。
- 真实无线电录音（噪声、多普勒频移、时钟差）**未做鲁棒处理**。
- **无斜切（slant）自动校正**。
- WAV：按 **PCM16** 处理；不处理 8/24/32bit、浮点或压缩格式（如 m4a）。
- `src/audio/tone.cpp` 当前未被使用（编码器用内部的 `append_tone_`）。
