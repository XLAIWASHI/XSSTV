#include <iostream>
#include "xsstv/AudioBuffer.h"

// 临时声明（因为 tone.h 和 wav.h 在 src 里）
namespace xsstv {
    void append_tone(AudioBuffer& audio, double freq, double duration_ms);
    bool write_wav(const std::string& path, const AudioBuffer& audio);
}

int main() {
    std::cout << "XSSTV CLI\n";

    xsstv::AudioBuffer audio;
    audio.sample_rate = 48000;

    // 生成 1 秒 1500 Hz 正弦波
    xsstv::append_tone(audio, 1500.0, 1000.0);
    xsstv::append_tone(audio, 1200.0, 1000.0);
    xsstv::append_tone(audio, 2000.0, 1000.0);
    xsstv::append_tone(audio, 440.0, 1000.0);
    xsstv::append_tone(audio, 20, 1000.0);

    std::cout << "采样点数：" << audio.mono.size() << "\n";
    std::cout << "时长：" << audio.mono.size() / double(audio.sample_rate) << " 秒\n";

    if (xsstv::write_wav("test_tone.wav", audio)) {
        std::cout << "已生成 test_tone.wav\n";
    } else {
        std::cout << "写入失败\n";
    }

    return 0;
}