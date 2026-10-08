#pragma once
#include "xsstv/Image.h"
#include "xsstv/Color.h"
#include "xsstv/AudioBuffer.h"

namespace xsstv
{

class Robot36
{
public:
    explicit Robot36(int sample_rate = 48000);
    AudioBuffer encode(const Image& image);
private:
    void encode_vis();
    void encode_line(const YCrCbImage& ycc, int line);
    void append_tone_(double freq, double duration_ms);

    int sample_rate_;
    double phase_ = 0.0;
    AudioBuffer audio_;
};

} // namespace xsstv
