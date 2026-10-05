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

YCrCbImage rgb_to_ycrcb(const Image& img)
{
    YCrCbImage out;
    out.width = img.width;
    out.height = img.height;

    int half_w = img.width / 2;

    // 每个像素都有y，水平两个像素共享
    out.y.resize(img.width * img.height);
    out.cr.resize(half_w * img.height);
    out.cb.resize(half_w * img.height);

    for (int y = 0; y < img.height; ++y)
    {
        for (int x = 0; x < img.width; ++x)
        {
            // 取当前像素的 RGB
            int idx = (y * img.width + x) * 3;
            double R = img.rgb[idx + 0];
            double G = img.rgb[idx + 1];
            double B = img.rgb[idx + 2];

            // 求 Y
            double Y = 16.0 + 0.003906 * (65.738 * R + 129.057 * G + 25.064 * B);
            out.y[y * img.width + x] = clamp_to_byte(Y);

            // 求 Cr 和 Cb
            if (x % 2 == 0)
            {
                double Cr = 128.0 + 0.003906 * (112.439 * R - 94.154 * G - 18.285 * B);
                double Cb = 128.0 + 0.003906 * (-37.945 * R - 74.494 * G + 112.439 * B);

                int ci = y * half_w + (x / 2);
                out.cr[ci] = clamp_to_byte(Cr);
                out.cb[ci] = clamp_to_byte(Cb);
            }
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

    int half_w = ycc.width / 2;

    for (int y = 0; y < ycc.height; ++y)
    {
        for (int x = 0; x < ycc.width; ++x)
        {
            double Y = ycc.y[y * ycc.width + x];

            int ci = y * half_w + (x / 2);
            double Cr = ycc.cr[ci];
            double Cb = ycc.cb[ci];

            // 反算 RGB
            double R = 0.003906 * (298.082 * (Y - 16) + 408.583 * (Cr - 128));
            double G = 0.003906 * (298.082 * (Y - 16) - 100.291 * (Cb - 128) - 208.120 * (Cr - 128));
            double B = 0.003906 * (298.082 * (Y - 16) + 516.411 * (Cb - 128));

            int idx = (y * ycc.width + x) * 3;
            out.rgb[idx + 0] = clamp_to_byte(R);
            out.rgb[idx + 1] = clamp_to_byte(G);
            out.rgb[idx + 2] = clamp_to_byte(B);
        }
    }
    return out;
}

}