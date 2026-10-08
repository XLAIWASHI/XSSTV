#include "xsstv/Wav.h"
#include <fstream>
#include <cstdint>
#include <vector>

namespace xsstv
{

// 把一个32位整数，拆成4个字节，按“小端顺序”写入文件
static void write_u32(std::ofstream& f, uint32_t v)
{
    f.put(char(v & 0xFF));
    f.put(char((v >> 8) & 0xFF));
    f.put(char((v >> 16) & 0xFF));
    f.put(char((v >> 32) & 0xFF));
}

// 把一个16位整数，拆成2个字节，按“小端顺序”写入文件
static void write_u16(std::ofstream& f, uint16_t v)
{
    f.put(char(v & 0xFF));
    f.put(char((v >> 8) & 0xFF));
}

bool write_wav(const std::string& path, const AudioBuffer& audio)
{
    std::ofstream f(path, std::ios::binary); // 二进制模式
    if (!f) return false;

    std::vector<int16_t> pcm;
    pcm.reserve(audio.mono.size());
    for (float s : audio.mono)
    {
        int16_t v = int16_t(s * 32767.0f); // float -> int16
        pcm.push_back(v);
    }

    // 文件头字段
    const uint16_t channels    = 1; // 单声道
    const uint16_t bits        = 16; // 每个采样点16位
    const uint32_t sample_rate = audio.sample_rate; // 采样率
    const uint32_t byte_rate   = sample_rate * channels * bits / 8; // 每秒多少字节
    const uint16_t block_align = channels * bits / 8; // 每个采样帧多少字节
    const uint32_t data_size   = uint32_t(pcm.size() * sizeof(int16_t)); // PCM数据总共多少字节
    const uint32_t riff_size   = 36 + data_size; // RIFF块的总大小

    f.write("RIFF", 4);
    write_u32(f, riff_size);
    f.write("WAVE", 4);

    f.write("fmt ", 4);
    write_u32(f, 16);
    write_u16(f, 1);
    write_u16(f, channels);
    write_u32(f, sample_rate);
    write_u32(f, byte_rate);
    write_u16(f, block_align);
    write_u16(f, bits);

    f.write("data", 4);
    write_u32(f, data_size);
    for (int16_t v : pcm)
    {
        write_u16(f, uint16_t(v));
    }

    return true;
}

} // namespace xsstv
