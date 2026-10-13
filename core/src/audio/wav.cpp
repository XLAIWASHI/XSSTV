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
    f.put(char((v >> 24) & 0xFF));
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

    // WAV 文件标准（PCM16）规定，音频数据必须用 16 位整数存储，而不是浮点数
    // 计算用 float 是为了精度，存储用 int16_t 是为了标准和省空间
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

// 从文件里读 4 个字节，按小端顺序拼成一个 32 位整数
static uint32_t read_u32(std::ifstream& f)
{
    uint8_t b[4];
    // 不改变内存内容，只改变“看待这块内存的方式”
    f.read(reinterpret_cast<char*>(b), 4);
    // 把 4 个字节拼成 32 位整数 小端
    return uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
}

// 从文件里读 2 个字节，按小端顺序拼成一个 16 位整数
static uint16_t read_u16(std::ifstream& f)
{
    uint8_t b[2];
    f.read(reinterpret_cast<char*>(b), 2);
    // 把 2 个字节拼成 16 位整数 小端
    return uint16_t(b[0] | (b[1] << 8));
}

// 相位法（正交解调）
// 因为像素时间太短，无法用数周期的方法测频率，
// 所以我们改用相位法，在每个采样点上算出瞬时频率，再平均。
// 这样能极大逼近真实值，但受限于编码时的有损压缩和传输噪声，100% 还原是不可能的，也不需要
bool read_wav(const std::string& path, AudioBuffer& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    // ignore(n) 跳过 n 个字节
    f.ignore(4);                   // "RIFF"
    read_u32(f);                   // riff_size 丢弃
    f.ignore(4);                   // "WAVE"
    f.ignore(4);                   // "fmt"
    read_u32(f);                   // fmt_size
    read_u16(f);                   // 编码 丢弃
    read_u16(f);                   // 声道 丢弃
    out.sample_rate = read_u32(f); // 采样率
    read_u32(f);                   // 字节率 丢弃
    read_u16(f);                   // 块对齐 丢弃
    read_u16(f);                   // 位深 丢弃
    f.ignore(4);                   // "data"

    // 将 pcm 由 uint16_t -> float
    const uint32_t data_size = read_u32(f); // 数据字节数
    const int n = int(data_size / 2);       // uint16_t 是 2 个字节， / 2 算出采样点总个数
    out.mono.resize(n);
    for (int i = 0; i < n; ++i)
    {
        int16_t v = int16_t(read_u16(f));
        out.mono[i] = v / 32767.0f;
    }
    return true;
}

} // namespace xsstv
