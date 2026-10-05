#include "tone.h"
#include <cmath>

namespace xsstv
{
    
void append_tone(AudioBuffer& audio, double freq, double duration_ms)
{
    const double two_pi = 2.0 * 3.14159265358979323846;
    double sr = double(audio.sample_rate); // 采样率
    int samples = int(sr * duration_ms / 1000.0); // 采样点数 = 采样率 * 时长(s)
    double phase_inc = two_pi * freq / sr; // 相位增量：每一个采样点，正弦波的相位走了多少弧度
    // 频率可以理解成1秒走了多少周期；所以two_pi * freq就是一共要走多少弧度的意思
    // 然后，采样率是1秒要采样的个数，所以就是每个采样点直接的差值

    for (int i = 0; i < samples; ++i)
    {
        double v = 0.5 * std::sin(phase_inc * i);
        // 1. 避免音量太大，播放时削顶
        // 2. 多个 tone 拼接时不会溢出
        // 3. 0.5 是常用的“安全音量”
        audio.mono.push_back(float(v));
    }
}

} // namespace xsstv
