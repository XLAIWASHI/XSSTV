#include "xsstv/Image.h"

#include <cstring>

#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "stb_image_resize2.h"

namespace xsstv
{
Image resize_image(const Image& img, int dst_w, int dst_h)
{
    Image dst;
    dst.width = dst_w;
    dst.height = dst_h;
    dst.rgb.resize(dst_w * dst_h * 3);      
    
    stbir_resize_uint8_linear(
        img.rgb.data(), img.width, img.height, 0,
        dst.rgb.data(), dst_w, dst_h, 0,
        STBIR_RGB);
    return dst;
}

// 保持宽高比缩放，剩下留黑边（letterbox）
Image fit_image(const Image& img, int dst_w, int dst_h)
{
    Image out;
    out.width = dst_w;
    out.height = dst_h;
    out.rgb.assign(size_t(dst_w) * dst_h * 3, 0); // 先铺黑底

    if (img.width <= 0 || img.height <= 0 || img.rgb.empty())
        return out;

    const double src_aspect = double(img.width) / double(img.height);
    const double dst_aspect = double(dst_w) / double(dst_h);

    int scaled_w, scaled_h;
    if (src_aspect > dst_aspect)
    {
        // 原图更宽：按宽度对齐，上下留黑边
        scaled_w = dst_w;
        scaled_h = int(dst_w / src_aspect + 0.5);
    }
    else
    {
        // 原图更高：按高度对齐，左右留黑边
        scaled_h = dst_h;
        scaled_w = int(dst_h * src_aspect + 0.5);
    }
    if (scaled_w < 1) scaled_w = 1;
    if (scaled_h < 1) scaled_h = 1;
    if (scaled_w > dst_w) scaled_w = dst_w;
    if (scaled_h > dst_h) scaled_h = dst_h;

    const Image scaled = resize_image(img, scaled_w, scaled_h);

    const int offset_x = (dst_w - scaled_w) / 2;
    const int offset_y = (dst_h - scaled_h) / 2;

    for (int y = 0; y < scaled_h; ++y)
    {
        const uint8_t* src = &scaled.rgb[size_t(y) * scaled_w * 3];
        uint8_t* dst = &out.rgb[(size_t(y + offset_y) * dst_w + offset_x) * 3];
        std::memcpy(dst, src, size_t(scaled_w) * 3);
    }
    return out;
}

} // namespace xsstv
