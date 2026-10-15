# Encoder_Summary

> 本文档只讲 Encoder 的实现细节。

---

## 1. 读取图像

**目标**：把任意常见格式（PNG/JPG/BMP）的图片，加载成 RGB 交错一维数组。

**用到什么**：
- `stb_image.h` / `stb_image_write.h`（单文件头库）
- 函数：`stbi_load`、`stbi_image_free`、`stbi_write_png`

**怎么实现**：
1. 在 `image_io.cpp` 里定义 `STB_IMAGE_IMPLEMENTATION`，展开 stb 的实现。
2. `stbi_load(path, &w, &h, &c, 3)` 读图，强制 3 通道。
3. 把 stb 返回的原始指针区间，拷贝到 `Image.rgb`（`std::vector<uint8_t>`）。
4. 调用 `stbi_image_free` 释放 stb 内部的内存。

**关键代码**：
```cpp
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, 3);
if (!data) return false;
out.width  = w;
out.height = h;
out.rgb.assign(data, data + w * h * 3);
stbi_image_free(data);
```

---

## 2. 缩放图片

**目标**：把任意尺寸的图，**保持宽高比**缩放到 `320×240` 以内，多余部分填**黑边**（居中放）。

**用到什么**：
- `stb_image_resize2.h`（`stbir_resize_uint8_linear`）——负责"纯拉伸缩放"
- `std::memcpy`（`<cstring>`）

**怎么实现**：
1. 先建一张 `320×240` 的**黑底图**：`out.rgb.assign(320*240*3, 0)`
2. 算原图宽高比 `src_aspect` 和目标的 `dst_aspect`
3. 谁更"宽"就按谁对齐，算出缩放后尺寸 `scaled_w × scaled_h`
4. 调 `resize_image` 把原图缩到 `scaled_w × scaled_h`
5. 算居中偏移 `offset_x/offset_y`，逐行 `memcpy` 贴到黑底上

**关键代码**：
```cpp
const double src_aspect = double(img.width) / img.height;
const double dst_aspect = double(dst_w) / dst_h;
int scaled_w, scaled_h;
if (src_aspect > dst_aspect) { scaled_w = dst_w; scaled_h = int(dst_w / src_aspect + 0.5); }
else                         { scaled_h = dst_h; scaled_w = int(dst_h * src_aspect + 0.5); }

const Image scaled = resize_image(img, scaled_w, scaled_h);   // 纯拉伸
const int ox = (dst_w - scaled_w) / 2, oy = (dst_h - scaled_h) / 2;
for (int y = 0; y < scaled_h; ++y)
    std::memcpy(&out.rgb[(size_t(y + oy) * dst_w + ox) * 3],
                &scaled.rgb[size_t(y) * scaled_w * 3], size_t(scaled_w) * 3);
```

> `resize_image` 是"纯拉伸到指定尺寸"；`fit_image` 在它基础上加"保比例 + 黑边"。

---

## 3. 颜色转换 RGB → YCrCb

**目标**：把 RGB 拆成"亮度 Y + 色差 Cr/Cb"，色差按 **4:2:0** 降采样。

**用到什么**：纯数学（`<cmath>`），**不依赖任何库**。

**怎么实现**：
1. **Y**：每个像素都算，`0.299R + 0.587G + 0.114B`
2. **色差**：把 **2×2 块求平均**后，再套 BT.601 公式
3. 结果 `clamp` 到 `0~255`
4. 尺寸：`y` 是 `w*h`，`cr/cb` 是 `(w/2)*(h/2)`

**关键代码**：
```cpp
double Y  = 0.299 * R + 0.587 * G + 0.114 * B;
double Cr = 0.500 * R - 0.418688 * G - 0.081312 * B + 128.0;
double Cb = -0.168736 * R - 0.331264 * G + 0.500 * B + 128.0;
// 色差：先对 2x2 块取平均 sr/sg/sb，再代入上面 Cr/Cb
```

> 为什么 +128：色差原始范围是 -128~127，+128 平移到 0~255，128 表示"中性/灰"。
> 2×2 平均 = 水平和垂直都降一半 → 这就是 4:2:0。

---

## 4. 频率映射（数值 → 频率）

**目标**：把 `0~255` 的数值，线性映射成 `1500~2300Hz`。

**用到什么**：无。

**怎么实现**：一条线性公式。

```cpp
const double f = FREQ_MIN + (v / 255.0) * (FREQ_MAX - FREQ_MIN);
// 0→1500(黑)  128→1900(灰)  255→2300(白)
```

---

## 5. 生成正弦波（`append_tone_`）

**目标**：往 `audio_` 末尾追加一段 **固定频率**、**相位连续**的正弦波。

**用到什么**：`std::sin`（`<cmath>`）。

**怎么实现**：
1. 采样点数 = `int(sample_rate_ * duration_ms / 1000)`
2. 每个采样点的相位增量 `inc = 2π * freq / sample_rate_`
3. 循环：写入 `0.5 * sin(phase_)`，然后 `phase_ += inc`

**关键代码**：
```cpp
const int samples = int(sample_rate_ * duration_ms / 1000.0);
const double inc = TWO_PI * freq / sample_rate_;
for (int i = 0; i < samples; ++i) {
    audio_.mono.push_back(float(0.5 * std::sin(phase_)));   // 0.5 幅度防削顶
    phase_ += inc;                                          // phase_ 是成员，跨调用累加
}
```

> ⚠️ 关键坑：`phase_` 必须是**成员变量**、跨段累加。每段从 0 开始会在拼接处**爆音**。
> （`tone.cpp` 里的 `append_tone` 就是每次从 0 开始，所以编码器没用它。）

---

## 6. 生成引导音 + VIS（`encode_vis`）

**目标**：生成"引导音 + 模式编号(VIS)"，让解码器知道"这是 Robot36"。

**用到什么**：`append_tone_`。

**怎么实现**（按顺序）：

```text
引导音 300ms @1900 → 中断 10ms @1200 → 引导音 300ms @1900
→ start bit 30ms @1200
→ 7 个数据位（LSB 优先，1=1100Hz / 0=1300Hz，每位 30ms）
→ 偶校验位 30ms
→ stop bit 30ms @1200
```

- VIS 码 `VIS_CODE = 8`
- 取位：`(vis >> i) & 1`（i 从 0 到 6，先低后高）
- 偶校验：统计 7 位里"1"的个数 `ones`，是奇数就发 `1`，使总数变偶

**关键代码**：
```cpp
append_tone_(FREQ_CENTER, TIME_LEADER);   // 1900, 300
append_tone_(FREQ_SYNC,   TIME_BREAK);    // 1200, 10
append_tone_(FREQ_CENTER, TIME_LEADER);
append_tone_(FREQ_SYNC,   TIME_VIS_START);
int ones = 0;
for (int i = 0; i < 7; ++i) {
    if ((VIS_CODE >> i) & 1) { append_tone_(FREQ_VIS_1, TIME_VIS_BIT); ++ones; }
    else                     { append_tone_(FREQ_VIS_0, TIME_VIS_BIT); }
}
append_tone_(ones % 2 ? FREQ_VIS_1 : FREQ_VIS_0, TIME_VIS_BIT); // 校验位
append_tone_(FREQ_SYNC, TIME_VIS_STOP);
```

---

## 7. 生成一行（`encode_line`）

**目标**：生成一行的 150ms 音频。

**用到什么**：`append_tone_` + 浮点位置累加。

**怎么实现**（6 段）：

| 段 | 频率 | 时长 |
|----|------|------|
| 同步 | 1200 | 9ms |
| 同步 porch | 1500 | 3ms |
| Y 扫描 | 1500→2300（320 像素） | 88ms |
| 分隔 | **偶行 1500 / 奇行 2300** | 4.5ms |
| 色差 porch | 1900 | 1.5ms |
| 色差扫描 | 1500→2300（160 样本） | 44ms |

- **Y 扫描**：每像素一个频率段；**用浮点位置累加** `pos += step`（`step = 0.275ms × fs`），边界用 `std::lround` 取整 → 每行精确 150ms。
- **色差**：偶数行取 `cr`、奇数行取 `cb`，行号取 `line/2`（相邻两行共享一组）。

**关键代码（Y 扫描）**：
```cpp
const double step = PIXEL_TIME * sample_rate_ / 1000.0;   // 每像素采样数(48k 时 = 13.2)
const uint8_t* y_row = &ycc.y[line * WIDTH];
double pos = 0.0;
for (int x = 0; x < WIDTH; ++x) {
    const double f   = FREQ_MIN + (y_row[x] / 255.0) * (FREQ_MAX - FREQ_MIN);
    const double inc = TWO_PI * f / sample_rate_;
    const int start = int(std::lround(pos)); pos += step;
    const int end   = int(std::lround(pos));
    for (int i = start; i < end; ++i) {
        audio_.mono.push_back(float(0.5 * std::sin(phase_)));
        phase_ += inc;
    }
}
```

> ⚠️ 坑：如果逐像素用 `int(samples)` 取整，会丢掉小数、每行变短、240 行累积漂移。所以必须用**浮点累加**。

---

## 8. 总调度（`encode`）

**目标**：把上面几步串起来。

**怎么实现**：
```cpp
const Image      scaled = fit_image(img, WIDTH, HEIGHT);  // 320×240
const YCrCbImage ycc    = rgb_to_ycrcb(scaled);
audio_.sample_rate = sample_rate_;
audio_.mono.clear();
phase_ = 0.0;                     // 整帧从头开始
encode_vis();
for (int line = 0; line < HEIGHT; ++line)
    encode_line(ycc, line);
return audio_;
```

---

## 9. 写 WAV（`write_wav`）

**目标**：把 `AudioBuffer`（float 单声道）写成 **PCM16 WAV**。

**用到什么**：`std::ofstream`（`<fstream>`）、`<cstdint>`。

**怎么实现**：
1. `float → int16`：`int16(s * 32767)`
2. 写 44 字节标准头：`RIFF` / 文件大小 / `WAVE` / `fmt ` / 编码 / 声道 / 采样率 / 字节率 / 块对齐 / 位深 / `data` / 数据大小
3. 数据按**小端**逐字节写（`write_u16` / `write_u32`）

**关键代码**：
```cpp
int16_t v = int16_t(s * 32767.0f);        // float → int16
f.write("RIFF", 4);
write_u32(f, 36 + data_size);
// ... fmt / data 头 ...
for (int16_t v : pcm) write_u16(f, uint16_t(v));
```

---

## 附：编码整体流程

```text
Image(RGB)
  → fit_image           320×240（保比例 + 黑边）
  → rgb_to_ycrcb        Y 320×240，Cr/Cb 160×120
  → encode_vis()        引导音 + VIS
  → 240 × encode_line() 每行 150ms
  → AudioBuffer
  → write_wav()         PCM16 WAV
```
