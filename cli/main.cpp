#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include "paths.h"
#include "xsstv/Image.h"
#include "xsstv/Robot36.h"
#include "xsstv/Wav.h"

void printHelp();
int robot36Encode(std::string in, std::string out, bool debug);

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;

    // 每次启动清空 temp, 并保证 output 存在
    fs::remove_all(paths::temp());
    fs::create_directories(paths::temp());
    fs::create_directories(paths::output());

    bool debug = false, help = false;
    std::string mode = "robot36"; // 默认robot36
    std::vector<std::string> pos; // 收集非 --debug 的参数
    
    // 收集参数
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d")
        {
            debug = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            help = true;
        }
        else if (arg == "--mode")
        {
            if (i + 1 < argc)
            {
                mode = argv[++i];
            }
        }
        else
        {
            pos.push_back(arg);
        }
    }

    // help
    if (help)
    {
        printHelp();
        return 0;
    }

    // 解析参数
    std::string cmd, input, output;
    if (!pos.empty())
    {
        cmd = pos[0];
    }
    input = (pos.size() > 1) ? pos[1] : "";
    output = (pos.size() > 2) ? pos[2] : "";

    // 解析路径
    std::string in_path;
    if (input.empty())
    {
        in_path = (paths::assets() / "test.png").string();
        std::cout << "提示：未指定输入，使用默认输入 " << in_path << std::endl;
    }
    else
    {
        fs::path p = input;
        if (fs::exists(p)) // 路径是否存在 不管它是绝对的还是相对的
        {
            in_path = p.string();
        }
        else // 路径不存在，当作文件名去assets里查找
        {
            in_path = (paths::assets() / input).string();
        }
    }

    std::string out_path;
    if (output.empty())
    {
        out_path = (paths::output() / "robot36.wav").string();
        std::cout << "提示：未指定输出，使用默认输出 " << out_path << std::endl;
    }
    else
    {
        out_path = output;
    }

    // 命令实现
    if (cmd == "encode")
    {
        if (mode == "robot36")
        {
            if (robot36Encode(in_path, out_path, debug))
                return 1;
            std::cout << "编码成功，当前模式：" << mode << std::endl;
        }
    }
    else if (cmd == "decode")
    {
        // TODO
    }
    else
    {
        std::cout << "no" << std::endl;
    }


    return 0;
}

void printHelp()
{
    std::cout <<
        "XSSTV - Robot36 编解码工具\n"
        "\n"
        "用法:\n"
        "  xsstv <命令> [选项] [输入] [输出]\n"
        "\n"
        "命令:\n"
        "  encode            图片 -> Robot36 WAV\n"
        "  decode            Robot36 WAV -> 图片\n"
        "\n"
        "选项:\n"
        "  -h, --help        显示本帮助并退出\n"
        "  -d, --debug       输出调试中间图到 temp/\n"
        "  --mode <name>     指定模式，目前仅支持 robot36（默认）\n"
        "\n"
        "参数:\n"
        "  输入              encode 默认 assets/test.png；decode 必填\n"
        "  输出              不指定时自动编号（robot36_NNNN.wav / decoded_NNNN.png）\n"
        "\n"
        "示例:\n"
        "  xsstv encode\n"
        "  xsstv encode photo.png\n"
        "  xsstv encode photo.png out.wav\n"
        "  xsstv decode output/robot36_0001.wav\n"
    << std::endl;
}

int robot36Encode(std::string in, std::string out, bool debug)
{
    xsstv::Image img;
    if (!xsstv::load_image(in, img))
    {
        std::cerr << "读图失败：" << in << std::endl;
        return 1;
    }
    std::cout << "读图成功：" << "长：" << img.width << "宽：" << img.height << std::endl;
    
    if (debug)
    {
        const xsstv::Image preview = xsstv::fit_image(img, 320, 240);
        xsstv::save_image((paths::temp() / "resized.png").string(), preview);
    }

    // 编码
    xsstv::Robot36 enc(48000);
    const xsstv::AudioBuffer audio = enc.encode(img);

    // 输出 wav
    if (!xsstv::write_wav(out, audio))
    {
        std::cerr << "写入wav错误：" << out << std::endl;
        return 1;
    }
    return 0;
}
