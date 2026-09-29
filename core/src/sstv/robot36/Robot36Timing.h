#pragma once

namespace xsstv
{
namespace sstv
{
namespace robot36
{
    // ==============================
    // 频率定义（Hz）
    // ==============================

    constexpr double FREQ_SYNC   = 1200.0;
    constexpr double FREQ_MIN    = 1500.0;
    constexpr double FREQ_MAX    = 2300.0;
    constexpr double FREQ_CENTER = 1900.0;


    // ==============================
    // 时间定义（ms）
    // ==============================

    constexpr double TIME_SYNC          = 9.0;
    constexpr double TIME_SYNC_PORCH    = 3.0;
    constexpr double TIME_Y_SCAN        = 88.0;
    constexpr double TIME_CHROMA_PORCH  = 1.5;
    constexpr double TIME_CHROMA_SCAN   = 44.0;


    // ==============================
    // 图像与协议
    // ==============================

    constexpr int WIDTH     = 320;
    constexpr int HEIGHT    = 240;

    constexpr int VIS_CODE  = 8;

} // namespace robot36
} // namespace sstv
} // namespace xsstv