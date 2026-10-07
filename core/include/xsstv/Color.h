#pragma once

#include "Image.h"
#include <vector>

namespace xsstv
{

struct YCrCbImage
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> y;  // width * height
    std::vector<uint8_t> cr; // (width/2) * (height/2)  4:2:0
    std::vector<uint8_t> cb; // (width/2) * (height/2)  4:2:0
};

YCrCbImage rgb_to_ycrcb(const Image& img);
Image ycrcb_to_rgb(const YCrCbImage& ycc);
}