#include "xsstv/Robot36.h"
#include "Robot36Timing.h"
#include <iostream>
#include <cmath>

namespace xsstv
{
// 把 Robot36 的协议常量（FREQ_* / TIME_* / WIDTH ...）引进来，方便直接使用
using namespace sstv::robot36;

static void demodulate_baseband(const std::vector<float>& x, int sample_rate,
                                double center_freq,
                                std::vector<double>& I, std::vector<double>& Q);
static std::vector<double> instantaneous_frequency( const std::vector<double>& I,
                                                    const std::vector<double>& Q,
                                                    int fs, double fc);
static uint8_t freq_to_value(double f);
static std::vector<int> find_line_syncs(const std::vector<double>& freq, int fs);
static std::vector<double> moving_average(const std::vector<double>& x, int L);
static double avg_freq(const std::vector<double>& freq, int a, int b);
static std::vector<uint8_t> blur3x3(const std::vector<uint8_t>& in, int w, int h);
static void fix_edges(std::vector<uint8_t>& p, int w, int h, int n);
namespace
{
    constexpr double TWO_PI = 6.283185307179586;
}

Robot36::Robot36(int sample_rate)
    : sample_rate_(sample_rate), phase_(0.0) {}

// 生成一段固定频率的正弦波。
// 相位使用成员 phase_ 累加，保证整帧音频在拼接处仍然连续（不会爆音）。
void Robot36::append_tone_(double freq, double duration_ms)
{
    const int samples = int(sample_rate_ * duration_ms / 1000.0);
    const double inc = TWO_PI * freq / sample_rate_;
    for (int i = 0; i < samples; ++i)
    {
        audio_.mono.push_back(float(0.5 * std::sin(phase_)));
        phase_ += inc;
    }
}

// 生成引导音 + VIS（说明对方"这是 Robot36"）
void Robot36::encode_vis()
{
    // 引导音 300ms @ 1900Hz
    append_tone_(FREQ_CENTER, TIME_LEADER);
    // 中断 10ms @ 1200Hz
    append_tone_(FREQ_SYNC, TIME_BREAK);
    // 引导音 300ms @ 1900Hz
    append_tone_(FREQ_CENTER, TIME_LEADER);
    // VIS start bit 30ms @ 1200Hz
    append_tone_(FREQ_SYNC, TIME_VIS_START);

    // 7 个数据位，LSB 优先
    // LSB 优先：先发最低位
    // Robot36 的 VIS 码是 8，二进制是 0001000（7位）。所以先发三个 0，再发一个 1，再发三个 0
    const int vis = VIS_CODE;
    int ones = 0;
    for (int i = 0; i < 7; ++i)
    {
        if ((vis >> i) & 1)
        {
            append_tone_(FREQ_VIS_1, TIME_VIS_BIT);
            ++ones;
        }
        else
        {
            append_tone_(FREQ_VIS_0, TIME_VIS_BIT);
        }
    }

    // 偶校验位
    if (ones % 2 != 0)
        append_tone_(FREQ_VIS_1, TIME_VIS_BIT);
    else
        append_tone_(FREQ_VIS_0, TIME_VIS_BIT);

    // VIS stop bit 30ms @ 1200Hz
    append_tone_(FREQ_SYNC, TIME_VIS_STOP);
}

// 生成一行（150ms）：同步 + porch + Y 扫描 + 分隔 + porch + 色差扫描
void Robot36::encode_line(const YCrCbImage& ycc, int line)
{
    // 同步脉冲 1200Hz 9ms
    append_tone_(FREQ_SYNC, TIME_SYNC);
    // 同步后的 porch 1500Hz 3ms 同步脉冲和图像数据之间的“缓冲”
    append_tone_(FREQ_MIN, TIME_SYNC_PORCH);

    // 每像素占用的采样点数（0.275ms），用浮点位置累加避免逐像素取整造成的时间漂移
    // 采样点本身必须是整数。 你不可能在 WAV 文件里存“半个采样点”
    // 但是，“每个像素占用多少个采样点”这个分配比例，是小数
    const double step = PIXEL_TIME * sample_rate_ / 1000.0;

    // Y 扫描：320 像素 / 88ms
    const uint8_t* y_row = &ycc.y[line * WIDTH];
    double pos = 0.0;
    for (int x = 0; x < WIDTH; ++x)
    {
        const double f = FREQ_MIN + (y_row[x] / 255.0) * (FREQ_MAX - FREQ_MIN); // Y 映射频率
        const double inc = TWO_PI * f / sample_rate_; // 每个采样点的间距
        const int start = int(std::lround(pos));
        pos += step;
        const int end = int(std::lround(pos));
        for (int i = start; i < end; ++i)
        {
            audio_.mono.push_back(float(0.5 * std::sin(phase_)));
            phase_ += inc;
        }
    }

    // 分隔脉冲：偶数行 1500Hz，奇数行 2300Hz（Robot36 靠它区分 R-Y / B-Y 行）
    append_tone_((line % 2 == 0) ? FREQ_MIN : FREQ_MAX, TIME_SEPARATOR);
    // 色差前的 porch 1900Hz 1.5ms
    append_tone_(FREQ_CENTER, TIME_CHROMA_PORCH);

    // 色差扫描：160 样本 / 44ms
    // 相邻两行共享一组色差，所以取 line/2 行；偶数行用 Cr，奇数行用 Cb
    const int half_w = WIDTH / 2;
    const uint8_t* c_row = (line % 2 == 0)
        ? &ycc.cr[(line / 2) * half_w]
        : &ycc.cb[(line / 2) * half_w];
    pos = 0.0;
    for (int x = 0; x < half_w; ++x)
    {
        const double f = FREQ_MIN + (c_row[x] / 255.0) * (FREQ_MAX - FREQ_MIN);
        const double inc = TWO_PI * f / sample_rate_;
        const int start = int(std::lround(pos));
        pos += step;
        const int end = int(std::lround(pos));
        for (int i = start; i < end; ++i)
        {
            audio_.mono.push_back(float(0.5 * std::sin(phase_)));
            phase_ += inc;
        }
    }

}

AudioBuffer Robot36::encode(const Image& img)
{
    // 统一缩放到 Robot36 的尺寸（保持宽高比，多余部分黑边），并转成 YCrCb
    const Image scaled = fit_image(img, WIDTH, HEIGHT);
    const YCrCbImage ycc = rgb_to_ycrcb(scaled);

    // 从头开始生成
    audio_.sample_rate = sample_rate_;
    audio_.mono.clear();
    phase_ = 0.0;

    // 引导 + VIS
    encode_vis();

    // 240 行图像数据
    for (int line = 0; line < HEIGHT; ++line)
    {
        encode_line(ycc, line);
    }

    return audio_;
}

Image Robot36::decode(const AudioBuffer &audio)
{
    // 拿频率
    const int fs = audio.sample_rate; // 48000
    const double ms = fs / 1000.0;    // 每毫秒多少采样点 = 48

    std::vector<double> I, Q;
    demodulate_baseband(audio.mono, fs, FREQ_CENTER, I, Q);
    std::vector<double> freq = instantaneous_frequency(I, Q, fs, FREQ_CENTER);

    // 先检测每一行的同步脉冲位置，用它来定位每一行
    // 这样即使开头有杂音、不从 VIS 开头、图像在录音中间、录音不完整，也能解码
    const std::vector<int> syncs = find_line_syncs(freq, fs);
    const bool use_syncs = (int(syncs.size()) >= HEIGHT);

    // 兜底方案：检测不到足够同步时，退回"假设从 VIS 头开始 + 固定时序"（自己编码器的时序是已知的）
    const int vis_samples = int(910 * ms); // 跳过 VIS 头
    const int delay = 47;                  // 两级滑动平均的群延迟：2*(L-1)/2

    // 准备 YCrCb
    YCrCbImage ycc;
    ycc.width = WIDTH;
    ycc.height = HEIGHT;
    ycc.y.resize(WIDTH * HEIGHT);
    ycc.cr.resize((WIDTH / 2) * (HEIGHT / 2));
    ycc.cb.resize((WIDTH / 2) * (HEIGHT / 2));

    const int half_w = WIDTH / 2;
    const double pix_step = PIXEL_TIME * ms;
    const int y_off = int(12 * ms);
    const int c_off = int(106 * ms);

    // 逐行采样
    for (int k = 0; k < HEIGHT; ++k)
    {
        // 用同步定位时，群延迟已经包含在检测到的位置里，不需要再补
        // 退回固定时序时，才需要额外补偿群延迟
        int line_start;
        int extra;
        if (use_syncs)
        {
            line_start = syncs[k];
            extra = 0;
        }
        else
        {
            line_start = vis_samples + k * int(TIME_LINE * ms);
            extra = delay;
        }

        // Y
        for (int i = 0; i < WIDTH; ++i)
        {
            int a = line_start + y_off + int(i * pix_step) + extra;
            int b = line_start + y_off + int((i + 1) * pix_step) + extra;
            ycc.y[k * WIDTH + i] = freq_to_value(avg_freq(freq, a, b));
        }

        // 色差
        std::vector<uint8_t>& plane = (k % 2 == 0) ? ycc.cr : ycc.cb;
        for (int j = 0; j < half_w; ++j)
        {
            int a = line_start + c_off + int(j * pix_step) + extra;
            int b = line_start + c_off + int((j + 1) * pix_step) + extra;
            plane[(k / 2) * half_w + j] = freq_to_value(avg_freq(freq, a, b));
        }
    }
    // 出图前先修掉扫描段首尾被污染的边缘色差，再做轻微平滑
    fix_edges(ycc.cr, half_w, HEIGHT / 2, 2);
    fix_edges(ycc.cb, half_w, HEIGHT / 2, 2);
    ycc.cr = blur3x3(ycc.cr, half_w, HEIGHT / 2);
    ycc.cb = blur3x3(ycc.cb, half_w, HEIGHT / 2);
    return ycrcb_to_rgb(ycc);
}

// 返回基带
// 原始信号只有一个投影，通过正交混频（乘 cos 和 -sin）得到 I/Q 两个坐标，
// 才能算出相位，进而由相位的变化速度求出频率
static void demodulate_baseband(const std::vector<float>& x, int sample_rate,
                                double center_freq,
                                std::vector<double>& I, std::vector<double>& Q)
{
    const size_t N = x.size();
    I.assign(N, 0.0);
    Q.assign(N, 0.0);

    // 未低通的 i 和 q
    std::vector<double> ir(N);
    std::vector<double> qr(N);

    // 混频
    const double two_pi = 6.283185307179586;
    // 计算本地振荡器（参考信号）每个采样点要推进多少相位
    const double dphi = two_pi * center_freq / sample_rate;
    double ref_phase = 0.0;
    for (size_t n = 0; n < N; ++n)
    {
        ir[n] = x[n] * std::cos(ref_phase);
        qr[n] = -x[n] * std::sin(ref_phase);
        ref_phase += dphi;
        if (ref_phase >= two_pi) ref_phase -= two_pi; // 超界复原
    }

    // 低通 + 滑动平均
    // 两级滑动平均
    const int L = 48;
    I = moving_average(ir, L);
    Q = moving_average(qr, L);
    I = moving_average(I, L);
    Q = moving_average(Q, L);
}

// 相位 + 解缠绕 + 差分
// fs 采样率 fc 中心频率
static std::vector<double> instantaneous_frequency( const std::vector<double>& I,
                                                    const std::vector<double>& Q,
                                                    int fs, double fc)
{
    const size_t N = I.size();
    const double PI = 3.141592653589793;
    std::vector<double> freq(N, fc);

    double prev = std::atan2(Q[0], I[0]); // 上一个采样点的原始相位
    for (size_t n = 1; n < N; ++n)
    {
        double raw = std::atan2(Q[n], I[n]); // 取第 n 个采样点的相位
        double d = raw - prev;
        if (d > PI) d -= 2 * PI;
        if (d < -PI) d += 2 * PI;
        prev = raw;
        freq[n] = fc + d * fs / (2.0 * PI);
    }
    return freq;
}

static uint8_t freq_to_value(double f)
{
    double v = (f - FREQ_MIN) / (FREQ_MAX - FREQ_MIN) * 255.0; // (f - 1500) / 800 * 255
    if (v < 0) return 0;
    if (v > 255) return 255;
    return uint8_t(v + 0.5);
}

// 在逐采样频率里找出每一行的同步脉冲位置
// Robot36 每行开头有一段约 9ms 的 1200Hz 同步脉冲，且行与行间隔固定为 150ms(7200 采样)
// 有了这些位置，就不必假设音频从 VIS 头开始，也能容忍开头杂音 / 图像在中间 / 录音不完整
// freq : 逐采样频率；fs : 采样率
// 返回 : 每一行同步脉冲的起始采样下标（按时间先后）
static std::vector<int> find_line_syncs(const std::vector<double>& freq, int fs)
{
    const double tol = 60.0;                            // 判定 1200Hz 的频率容差（要窄于 VIS 数据位的 1100/1300）
    const int min_run = int(5.0 * fs / 1000.0);        // 脉冲最短 5ms 才算数
    const int line_len = int(TIME_LINE * fs / 1000.0); // 一行 150ms = 7200 采样
    const int spacing_tol = line_len / 50;             // 行距允许约 2% 误差

    // 第一步：找出所有 1200Hz 脉冲的起点（VIS 里的 1200Hz 段也会被算进来）
    std::vector<int> runs;
    size_t n = 0;
    while (n < freq.size())
    {
        if (std::fabs(freq[n] - FREQ_SYNC) < tol)
        {
            size_t end = n;
            while (end < freq.size() && std::fabs(freq[end] - FREQ_SYNC) < tol)
                ++end;
            if (int(end - n) >= min_run)
                runs.push_back(int(n));
            n = end;
        }
        else
        {
            ++n;
        }
    }

    // 第二步：从所有脉冲里挑出"间隔约等于一行(7200)"的最长序列，那就是真正的行同步
    int best_start = 0;
    int best_len = 0;
    for (size_t i = 0; i < runs.size(); ++i)
    {
        size_t j = i;
        while (j + 1 < runs.size())
        {
            const int diff = runs[j + 1] - runs[j];
            if (diff < line_len - spacing_tol || diff > line_len + spacing_tol)
                break;
            ++j;
        }
        const int len = int(j - i) + 1;
        if (len > best_len)
        {
            best_len = len;
            best_start = int(i);
        }
    }

    // 第三步：向前补齐 + 返回
    // VIS 的 stop bit(1200Hz) 会和第一行的同步脉冲连成一整段，导致第一行没被当成独立脉冲；
    // 用同样的行距往前推，把前面缺的行补回来
    std::vector<int> syncs;
    if (runs.empty())
        return syncs;

    int first = runs[best_start];
    while (first - line_len >= 0 && std::fabs(freq[first - line_len] - FREQ_SYNC) < tol)
        first -= line_len;

    for (int p = first; p < runs[best_start]; p += line_len)
        syncs.push_back(p);
    for (int k = 0; k < best_len; ++k)
        syncs.push_back(runs[best_start + k]);
    return syncs;
}

static std::vector<double> moving_average(const std::vector<double>& x, int L)
{
    std::vector<double> y(x.size());
    double sum = 0.0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        sum += x[n];
        if (n >= (size_t)L) sum -= x[n - L];
        const int cnt = (n < (size_t)L) ? int(n) + 1 : L;
        y[n] = sum / cnt;
    }
    return y;
}

static double avg_freq(const std::vector<double>& freq, int a, int b)
{
    if (a < 0) a = 0;
    if (b > (int)freq.size()) b = (int)freq.size();
    if (b <= a) return FREQ_MIN;
    double s = 0.0;
    for (int t = a; t < b; ++t) s += freq[t];
    return s / (b - a);
}

// 把平面最外的 n 列/行，用相邻内侧的值覆盖
// 用来去掉扫描段首尾被 sync/porch 污染造成的彩色边缘
static void fix_edges(std::vector<uint8_t>& p, int w, int h, int n)
{
    for (int y = 0; y < h; ++y)          // 左右
        for (int k = 0; k < n; ++k)
        {
            p[y * w + k] = p[y * w + n];            // 左边
            p[y * w + (w - 1 - k)] = p[y * w + (w - 1 - n)]; // 右边
        }
    for (int x = 0; x < w; ++x)          // 上下
        for (int k = 0; k < n; ++k)
        {
            p[k * w + x] = p[n * w + x];            // 上边
            p[(h - 1 - k) * w + x] = p[(h - 1 - n) * w + x]; // 下边
        }
}

static std::vector<uint8_t> blur3x3(const std::vector<uint8_t>& in, int w, int h)
{
    std::vector<uint8_t> out(in.size());
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int s = 0, c = 0;
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    int yy = y + dy, xx = x + dx;
                    if (yy < 0 || yy >= h || xx < 0 || xx >= w) continue;
                    s += in[yy * w + xx];
                    ++c;
                }
            }
            out[y * w + x] = uint8_t(s / c);
        }
    }
    return out;
}

} // namespace xsstv
