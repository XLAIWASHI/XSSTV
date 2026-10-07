#include "xsstv/Robot36.h"
#include "Robot36Timing.h"

#include <cmath>

namespace xsstv
{
// 把 Robot36 的协议常量（FREQ_* / TIME_* / WIDTH ...）引进来，方便直接使用
using namespace sstv::robot36;

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
    // 同步后的 porch 1500Hz 3ms
    append_tone_(FREQ_MIN, TIME_SYNC_PORCH);

    // 每像素占用的采样点数（0.275ms），用浮点位置累加避免逐像素取整造成的时间漂移
    const double step = PIXEL_TIME * sample_rate_ / 1000.0;

    // Y 扫描：320 像素 / 88ms
    const uint8_t* y_row = &ycc.y[line * WIDTH];
    double pos = 0.0;
    for (int x = 0; x < WIDTH; ++x)
    {
        const double f = FREQ_MIN + (y_row[x] / 255.0) * (FREQ_MAX - FREQ_MIN);
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
        encode_line(ycc, line);

    return audio_;
}

} // namespace xsstv
