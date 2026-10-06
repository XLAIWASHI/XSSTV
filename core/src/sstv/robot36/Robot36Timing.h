#pragma once

namespace xsstv
{
namespace sstv
{
namespace robot36
{
    // 频率定义（Hz）
    constexpr double FREQ_SYNC   = 1200.0; // 同步脉冲
    constexpr double FREQ_MIN    = 1500.0; // 黑色电平
    constexpr double FREQ_MAX    = 2300.0; // 白色电平
    constexpr double FREQ_CENTER = 1900.0; // 中心频率
    constexpr double FREQ_VIS_1  = 1100.0; // VIS 比特 “1”
    constexpr double FREQ_VIS_0  = 1300.0; // VIS 比特 “0”

    // 时间定义（ms）
    constexpr double TIME_SYNC          = 9.0;  // 每行开头的 1200Hz 同步脉冲
    constexpr double TIME_SYNC_PORCH    = 3.0;  // 同步之后的 1500Hz 过渡
    constexpr double TIME_Y_SCAN        = 88.0; // Y亮度扫描，320像素
    constexpr double TIME_SEPARATOR     = 4.5;  // Y和色差之间的分隔脉冲
    constexpr double TIME_CHROMA_PORCH  = 1.5;  // 色差扫描前的 1900Hz 过渡
    constexpr double TIME_CHROMA_SCAN   = 44.0; // 色差扫描，160像素

    // VIS 时间 （ms）
    constexpr double TIME_LEADER    = 300.0; // 引导音
    constexpr double TIME_BREAK     = 10.0;  // 中断
    constexpr double TIME_VIS_START = 30.0;  // VIS start bit
    constexpr double TIME_VIS_BIT   = 30.0;  // 每个比特
    constexpr double TIME_VIS_STOP  = 30.0;  // VIS stop bit

    // 一行总时间（编译器自动算好 150.0）
    constexpr double TIME_LINE = TIME_SYNC + TIME_SYNC_PORCH + TIME_Y_SCAN
                               + TIME_SEPARATOR + TIME_CHROMA_PORCH + TIME_CHROMA_SCAN;

    // 图像与协议
    constexpr int WIDTH         = 320;   // 图像宽度（Y每行像素数）
    constexpr int HEIGHT        = 240;   // 图像高度（总行数）
    constexpr double PIXEL_TIME = 0.275; // 每个像素的时间
    constexpr int VIS_CODE      = 8;     // Robot36 的 VIS 代码

} // namespace robot36
} // namespace sstv
} // namespace xsstv