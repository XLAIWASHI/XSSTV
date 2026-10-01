#include <iostream>
#include "xsstv/Image.h"

int main()
{
    std::cout << "XSSTV CLI" << std::endl;

    xsstv::Image img;
    if (!xsstv::load_image("assets/test.png", img))
    {
        std::cout << "读图失败" << std::endl;
        return 1;
    }

    std::cout << "读到图片：" << img.width << "x " << img.height << std::endl;

    if (xsstv::save_image("copy.png", img))
    {
        std::cout << "已保存 copy.png" << std::endl;
    }

    return 0;
}