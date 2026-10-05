#pragma once

#include "Image.h"
#include <vector>

namespace xsstv
{

struct YCrCbImage
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> y; // 320*240
    std::vector<uint8_t> cr; // 160*240
    std::vector<uint8_t> cb; // 160*240
};

YCrCbImage rgb_to_ycrcb(const Image& img);
Image ycrcb_to_rgb(const YCrCbImage& ycc);
}