#include "xsstv/Image.h"

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

}