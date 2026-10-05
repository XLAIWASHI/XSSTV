#pragma once

#include <vector>

namespace xsstv
{
    
struct AudioBuffer
{
    int sample_rate = 48000; // 采样率
    std::vector<float> mono; // 采样点数组
};

} // namespace xsstv
