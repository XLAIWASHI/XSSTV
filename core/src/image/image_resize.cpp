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
    // data 返回 uint8_t* 指针
    stbir_resize_uint8_linear(
        img.rgb.data(), img.width, img.height, 0,
        dst.rgb.data(), dst_w, dst_h, 0,
        STBIR_RGB
    );
    return dst;
}

// 保持宽高比缩放，剩下留黑边（letterbox）
Image fit_image(const Image& img, int dst_w, int dst_h)
{
    Image out;
    out.width = dst_w;
    out.height = dst_h;
    // 铺黑底
    // 清空 vector 里原有的数据，然后重新分配指定数量的元素，并全部填入指定的初始值
    out.rgb.assign(size_t(dst_w) * dst_h * 3, 0);

    if (img.width <= 0 || img.height <= 0 || img.rgb.empty())
        return out;
    
    const double src_aspect = double(img.width) / double(img.height); // 源宽高比
    const double dst_aspect = double(dst_w) / double(dst_h); // 目标宽高比

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

    // 缩放后居中到目标画布上
    const Image scaled = resize_image(img, scaled_w, scaled_h);

    const int offset_x = (dst_w - scaled_w) / 2;
    const int offset_y = (dst_h - scaled_h) / 2;

    for (int y = 0; y < scaled_h; ++y)
    {
        // src 和 dst 是首地址
        const uint8_t* src = &scaled.rgb[size_t(y) * scaled_w * 3];
        uint8_t* dst = &out.rgb[(size_t(y + offset_y) * dst_w + offset_x) * 3];
        std::memcpy(dst, src, size_t(scaled_w) * 3);
        // 从 src 这个位置开始，读取 size_t(scaled_w) * 3 个字节的数据
        // 然后把这些数据原样写入到 dst 这个位置开始的内存里
    }
    // 也就是说，我们最开始让out变黑，然后out的长度位320*240
    // 然后我们缩放图片，最终的数据位scaled，最后让scaled覆盖out的对应区域
    return out;
}

} // namespace xsstv
