# Decoder_Summary

> 本文档只讲 Decoder 的实现细节。

---

## 1. 解码思路（总流程）

```text
AudioBuffer(单声道 float)
  → demodulate_baseband     正交混频 + 低通，得到基带 I/Q
  → instantaneous_frequency 相位解缠 + 差分，得到逐采样频率 freq[]
  → find_line_syncs         找每行的 1200Hz 同步脉冲位置
  → decode 主循环           逐行采样 Y / 色差 → 组装 YCrCb
  → fix_edges + blur3x3     修边缘 + 平滑色差
  → ycrcb_to_rgb            转回 RGB
Image(RGB)
```

核心思想：**把音频的"频率"还原成"像素值"**。音频频率 1500~2300Hz ↔ 像素值 0~255。

---

## 2. 正交解调（`demodulate_baseband`）

**目标**：把一条"频率在 1500~2300Hz 之间变化的实信号"，搬移到 0Hz 附近，得到**复数包络** I/Q。

**用到什么**：`std::cos` / `std::sin`（`<cmath>`）、`moving_average`。

**怎么实现**：
1. **参考振荡器**：`ref_phase` 每个采样点累加 `2π·fc/fs`（fc=1900），并对 2π 取模
2. **混频**：`ir = x·cos(ref_phase)`，`qr = -x·sin(ref_phase)`
3. **低通**：对 ir/qr 各做**两级滑动平均** → I、Q

**关键代码**：
```cpp
const double dphi = two_pi * center_freq / sample_rate;
double ref_phase = 0.0;
for (size_t n = 0; n < N; ++n) {
    ir[n] = x[n] * std::cos(ref_phase);
    qr[n] = -x[n] * std::sin(ref_phase);
    ref_phase += dphi;
    if (ref_phase >= two_pi) ref_phase -= two_pi;   // 取模，防精度崩
}
I = moving_average(ir, 48);
Q = moving_average(qr, 48);
I = moving_average(I, 48);   // 第二级（级联），加强对镜像的衰减
Q = moving_average(Q, 48);
```

**为什么要这么做**：
- 混频后会产生两个分量：**差频 `f-fc`（基带，我们要的）** 和 **和频 `f+fc`（约 3400~4200Hz，镜像，要滤掉）**。
- 低通把镜像滤掉，剩下 `I+jQ = (A/2)·e^{j·2π(f-fc)t}` —— 它的**相位以 `(f-fc)` 旋转**。

**关键坑**：
- `ref_phase` 必须**累加并取模**。若直接算 `2π·fc·n/fs`，n 很大时 `cos/sin` 精度崩。
- 低通只做一级时，3400~4200Hz 的镜像只能衰减约 20~27dB → **残留约 5%** → 频率抖动 ±190Hz → 图上来回**横条纹、偏色**。所以做了**两级级联**。

---

## 3. 相位 + 解缠 → 瞬时频率（`instantaneous_frequency`）

**目标**：从 I/Q 算出**每个采样点的瞬时频率**。

**用到什么**：`std::atan2`（`<cmath>`）。

**怎么实现**：
1. 相位 `θ = atan2(Q, I)`（范围 -π~π）
2. **解缠绕**：相邻相位差 `d = θ[n] - θ[n-1]`；若 `|d| > π` 说明绕了一圈，`d ∓ 2π` 修正
3. **差分求频**：`freq[n] = fc + d·fs/(2π)`

**关键代码**：
```cpp
double prev = std::atan2(Q[0], I[0]);
for (size_t n = 1; n < N; ++n) {
    double raw = std::atan2(Q[n], I[n]);
    double d = raw - prev;
    if (d >  PI) d -= 2 * PI;      // 解缠绕
    if (d < -PI) d += 2 * PI;
    prev = raw;
    freq[n] = fc + d * fs / (2.0 * PI);   // 相位变化速度 = 频率偏移
}
```

> `atan2` 每次只返回 -π~π，超过就"跳回"，画出来是锯齿。解缠绕就是把它还原成连续曲线。

---

## 4. 频率 → 数值（`freq_to_value`）

**目标**：频率 → 0~255 像素值（编码的逆运算）。

```cpp
double v = (f - FREQ_MIN) / (FREQ_MAX - FREQ_MIN) * 255.0;   // (f-1500)/800*255
if (v < 0) return 0;
if (v > 255) return 255;
return uint8_t(v + 0.5);
```

---

## 5. 同步检测（`find_line_syncs`）

**目标**：在 `freq[]` 里找出**每一行的同步脉冲位置**（每行开头约 9ms 的 1200Hz）。

**为什么需要它**：有了它，解码就**不依赖"音频从 VIS 头开始"**，能容忍：开头有杂音、不从 VIS 开头、图像在录音中间、录音不完整。

**怎么实现**：
1. 找出所有 1200Hz 的连续脉冲（容差 `tol=60`，最短 `min_run=5ms`）
2. 从这些脉冲里，挑出"**间隔约等于一行(`line_len`)**"的**最长序列** → 就是行同步
3. **向前补齐**：VIS 的 stop bit(1200Hz) 会和第一行的同步连成一整段，导致第一行被漏掉，用行距往前推补回来

**关键参数**：
- `line_len = int(150ms × fs/1000)`（48k 时 = 7200）
- `tol = 60`：要**窄于 VIS 数据位的 1100/1300**，否则会把数据位误判成同步

**关键代码**：
```cpp
const int line_len = int(TIME_LINE * fs / 1000.0);
// 第一步：找所有 1200Hz 脉冲起点
while (n < freq.size()) {
    if (std::fabs(freq[n] - FREQ_SYNC) < tol) {
        size_t end = n;
        while (end < freq.size() && std::fabs(freq[end] - FREQ_SYNC) < tol) ++end;
        if (int(end - n) >= min_run) runs.push_back(int(n));
        n = end;
    } else ++n;
}
// 第二步：挑"间隔约一行"的最长序列
// 第三步：向前补齐（VIS stop bit 与第一行连段）
```

---

## 6. 逐行采样（`decode` 主体）

**目标**：把频率还原成一张 320×240 的 YCrCb 图。

**怎么实现**：
1. 算好常量：`fs`、`ms = fs/1000`
2. `demodulate_baseband` → I/Q；`instantaneous_frequency` → `freq`
3. `find_line_syncs` → `syncs`
4. 定位方式：
   - 找到 ≥240 个同步 → **每行用第 k 个 sync 定位**
   - 否则**退回固定时序**：跳过 VIS(910ms)，每行 150ms，补群延迟 `delay=47`
5. 逐行采样（行内偏移）：
   - `y_off = int(12ms × ms)`（同步 9ms + porch 3ms）
   - `c_off = int(106ms × ms)`（9+3+88+4.5+1.5）
   - `pix_step = PIXEL_TIME × ms`（0.275ms × ms）
6. **Y**：像素 i 覆盖范围 `[i·step, (i+1)·step)` → `avg_freq` 求平均 → `freq_to_value`
7. **色差**：偶数行写 `cr`、奇数行写 `cb`，行号 `k/2`（相邻两行共享）

**关键代码**：
```cpp
for (int k = 0; k < HEIGHT; ++k) {
    int line_start, extra;
    if (use_syncs) { line_start = syncs[k]; extra = 0; }      // 同步定位，延迟已含在内
    else { line_start = vis_samples + k * int(TIME_LINE*ms); extra = delay; }

    // Y
    for (int i = 0; i < WIDTH; ++i) {
        int a = line_start + y_off + int(i * pix_step) + extra;
        int b = line_start + y_off + int((i + 1) * pix_step) + extra;
        ycc.y[k * WIDTH + i] = freq_to_value(avg_freq(freq, a, b));
    }
    // 色差：偶行 Cr / 奇行 Cb，存第 k/2 行
    std::vector<uint8_t>& plane = (k % 2 == 0) ? ycc.cr : ycc.cb;
    for (int j = 0; j < half_w; ++j) {
        int a = line_start + c_off + int(j * pix_step) + extra;
        int b = line_start + c_off + int((j + 1) * pix_step) + extra;
        plane[(k / 2) * half_w + j] = freq_to_value(avg_freq(freq, a, b));
    }
}
```

---

## 7. 画质处理（几个辅助函数）

| 函数 | 作用 |
|------|------|
| `avg_freq(freq, a, b)` | 取某像素覆盖范围内频率的**平均**（降噪；越界返回 FREQ_MIN） |
| `moving_average(x, L)` | 滑动平均（低通用，`L=48`） |
| `fix_edges(p, w, h, n)` | 把平面最外 n 列/行，用**相邻内侧值覆盖**（去边缘彩色） |
| `blur3x3(in, w, h)` | 色差平面 **3×3 平滑**（压彩色噪点/条纹） |

**顺序很重要**：先 `fix_edges` **再** `blur3x3`。
因为扫描段首尾会被相邻音（sync/porch）污染，`fix_edges` 先把脏边替换掉；若先模糊，会把脏边扩散进来。

```cpp
fix_edges(ycc.cr, half_w, HEIGHT / 2, 2);
fix_edges(ycc.cb, half_w, HEIGHT / 2, 2);
ycc.cr = blur3x3(ycc.cr, half_w, HEIGHT / 2);
ycc.cb = blur3x3(ycc.cb, half_w, HEIGHT / 2);
```

---

## 8. 出图（`ycrcb_to_rgb`）

**目标**：YCrCb → RGB，得到最终 `Image`。

```cpp
R = Y + 1.402 (Cr-128)
G = Y - 0.344136 (Cb-128) - 0.714136 (Cr-128)
B = Y + 1.772 (Cb-128)
```

色差是半分辨率（160×120），还原时按 `(x/2, y/2)` 取。

---

## 9. 已知限制

- **只支持 Robot36**（PD120/Martin 等需另写协议）。
- **无频率偏移/多普勒补偿**、**无斜切(slant)校正** → 真实无线电录音（噪声大、频移、时钟差）可能解不好。
- 对**自己编码**或**其它软件生成的干净 Robot36** 效果良好。
- 依赖采样率正确：若源文件不是 48k，要保证 `read_wav` 把采样率读对（解码按 `ms=fs/1000` 自适应）。
