#include <iostream>
#include <string>
#include <filesystem>

#include "paths.h"
#include "xsstv/Image.h"
#include "xsstv/Robot36.h"
#include "xsstv/Wav.h"

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;

    // --debug 时把中间图落到 temp/ 便于观察
    const bool debug = (argc > 1 && std::string(argv[1]) == "--debug");

    // 每次启动清空 temp，并保证 output 存在
    fs::remove_all(paths::temp());
    fs::create_directories(paths::temp());
    fs::create_directories(paths::output());

    // 输入素材
    const std::string in = (paths::assets() / "test.png").string();

    xsstv::Image img;
    if (!xsstv::load_image(in, img))
    {
        std::cerr << "读图失败: " << in << "\n";
        return 1;
    }
    std::cout << "读到图片 " << img.width << "x" << img.height << "\n";

    if (debug)
    {
        const xsstv::Image preview = xsstv::fit_image(img, 320, 240);
        xsstv::save_image((paths::temp() / "resized.png").string(), preview);
    }

    // 编码：Image -> Robot36 音频
    xsstv::Robot36 enc(48000);
    const xsstv::AudioBuffer audio = enc.encode(img);

    // 输出 WAV
    const std::string wav_out = (paths::output() / "robot36.wav").string();
    if (!xsstv::write_wav(wav_out, audio))
    {
        std::cerr << "写 WAV 失败: " << wav_out << "\n";
        return 1;
    }

    std::cout << "已生成 " << wav_out << "\n";
    std::cout << "时长 " << audio.mono.size() / 48000.0
              << " 秒, 采样点 " << audio.mono.size() << "\n";

    return 0;
}
