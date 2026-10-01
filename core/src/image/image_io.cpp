#include "xsstv/Image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace xsstv
{

    bool load_image(const std::string& path, Image& out)
    {
        int w = 0, h = 0, c = 0;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, 3);
        if (!data) return false;

        out.width = w;
        out.height = h;
        out.rgb.assign(data, data + w * h * 3);
        stbi_image_free(data);
        return true;
    }

    bool save_image(const std::string& path, const Image& img)
    {
        return stbi_write_png(path.c_str(),
                                img.width, img.height, 3,
                                img.rgb.data(),
                                img.width * 3) != 0;
    }
}