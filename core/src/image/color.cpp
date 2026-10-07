#include "xsstv/Color.h"

namespace xsstv
{
// 把浮点数钳位到 0-255 并转换成 uint8_t
static uint8_t clamp_to_byte(double v)
{
    if (v < 0.0) return 0;
    if (v > 255.0) return 255;
    return uint8_t(v + 0.5); // 四舍五入
}

// RGB -> YCrCb（全范围 0~255，标准 BT.601 / JPEG 系数）
// 色差按 4:2:0 降采样：水平、垂直都取 2x2 块的平均，所以 cr/cb 是 (w/2)*(h/2)
YCrCbImage rgb_to_ycrcb(const Image& img)
{
    YCrCbImage out;
    out.width = img.width;
    out.height = img.height;

    const int half_w = img.width / 2;
    const int half_h = img.height / 2;

    out.y.resize(img.width * img.height);
    out.cr.resize(half_w * half_h);
    out.cb.resize(half_w * half_h);

    // 亮度：每个像素一个值
    for (int y = 0; y < img.height; ++y)
    {
        for (int x = 0; x < img.width; ++x)
        {
            const int idx = (y * img.width + x) * 3;
            const double R = img.rgb[idx + 0];
            const double G = img.rgb[idx + 1];
            const double B = img.rgb[idx + 2];

            const double Y = 0.299 * R + 0.587 * G + 0.114 * B;
            out.y[y * img.width + x] = clamp_to_byte(Y);
        }
    }

    // 色差：2x2 块求平均后再算，写到半分辨率平面
    for (int j = 0; j < half_h; ++j)
    {
        for (int i = 0; i < half_w; ++i)
        {
            double sr = 0.0, sg = 0.0, sb = 0.0;
            for (int dy = 0; dy < 2; ++dy)
            {
                for (int dx = 0; dx < 2; ++dx)
                {
                    const int x = i * 2 + dx;
                    const int y = j * 2 + dy;
                    const int idx = (y * img.width + x) * 3;
                    sr += img.rgb[idx + 0];
                    sg += img.rgb[idx + 1];
                    sb += img.rgb[idx + 2];
                }
            }
            sr *= 0.25;
            sg *= 0.25;
            sb *= 0.25;

            const double Cr = 0.500 * sr - 0.418688 * sg - 0.081312 * sb + 128.0;
            const double Cb = -0.168736 * sr - 0.331264 * sg + 0.500 * sb + 128.0;

            const int ci = j * half_w + i;
            out.cr[ci] = clamp_to_byte(Cr);
            out.cb[ci] = clamp_to_byte(Cb);
        }
    }

    return out;
}

Image ycrcb_to_rgb(const YCrCbImage& ycc)
{
    Image out;
    out.width = ycc.width;
    out.height = ycc.height;
    out.rgb.resize(ycc.width * ycc.height * 3);

    const int half_w = ycc.width / 2;

    for (int y = 0; y < ycc.height; ++y)
    {
        for (int x = 0; x < ycc.width; ++x)
        {
            const double Y = ycc.y[y * ycc.width + x];

            // 色差是半分辨率，还原时用 (x/2, y/2) 去取
            const int ci = (y / 2) * half_w + (x / 2);
            const double Cr = ycc.cr[ci];
            const double Cb = ycc.cb[ci];

            const double R = Y + 1.402 * (Cr - 128.0);
            const double G = Y - 0.344136 * (Cb - 128.0) - 0.714136 * (Cr - 128.0);
            const double B = Y + 1.772 * (Cb - 128.0);

            const int idx = (y * ycc.width + x) * 3;
            out.rgb[idx + 0] = clamp_to_byte(R);
            out.rgb[idx + 1] = clamp_to_byte(G);
            out.rgb[idx + 2] = clamp_to_byte(B);
        }
    }
    return out;
}

} // namespace xsstv
