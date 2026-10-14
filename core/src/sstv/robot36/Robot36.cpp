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

    const int vis_samples = int(910 * ms); // 跳过 VIS 头

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
        const int line_start = vis_samples + k * int(TIME_LINE * ms);

        // Y
        for (int i = 0; i < WIDTH; ++i)
        {
            int idx = line_start + y_off + int((i + 0.5) * pix_step);
            if (idx < 0 || idx >= int(freq.size())) continue;
            ycc.y[k * WIDTH + i] = freq_to_value(freq[idx]);
        }

        // 色差
        std::vector<uint8_t>& plane = (k % 2 == 0) ? ycc.cr : ycc.cb;
        for (int j = 0; j < half_w; ++j)
        {
            int idx = line_start + c_off + int((j + 0.5) * pix_step);
            if (idx < 0 || idx >= int(freq.size())) continue;
            plane[(k / 2) * half_w + j] = freq_to_value(freq[idx]);
        }
    }
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
    const int L = 48; // 窗口
    double sum_i = 0.0, sum_q = 0.0;
    for (size_t n = 0; n < N; ++n)
    {
        sum_i += ir[n];
        sum_q += qr[n];
        if (n >= (size_t)L)
        {
            sum_i -= ir[n - L];
            sum_q -= qr[n - L];
        }
        const int cnt = (n < (size_t)L) ? int(n) + 1 : L;
        I[n] = sum_i / cnt;
        Q[n] = sum_q / cnt;
    }

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

} // namespace xsstv
