#pragma once

#include <QImage>

#include "xsstv/Image.h"

// 仅用于显示：把 Core 的 xsstv::Image(RGB888) 转成 QImage。
// 只是"展示适配"，不实现任何图像算法。
inline QImage toQImage(const xsstv::Image& img)
{
    if (img.width <= 0 || img.height <= 0 || img.rgb.empty())
        return QImage();
    QImage tmp(img.rgb.data(), img.width, img.height, img.width * 3, QImage::Format_RGB888);
    return tmp.copy(); // 源缓冲可能被复用，拷一份
}
