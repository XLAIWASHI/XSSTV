#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xsstv
{

// 通用图像结构
struct Image
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgb;
};

// 读写图片
bool load_image(const std::string& path, Image& out);
bool save_image(const std::string& path, const Image& img);
Image resize_image(const Image& img, int dst_w, int dst_h);

// 保持宽高比缩放到 dst 尺寸内，多余的部分用黑边填充
Image fit_image(const Image& img, int dst_w, int dst_h);
}