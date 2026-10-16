#include "xsstv/Wav.h"
#include <fstream>
#include <cstdint>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace xsstv
{

#ifdef _WIN32
// UTF-8 路径 → 宽字符。Windows 用宽字符文件 API 才能打开中文路径
static std::wstring toWide(const std::string& utf8)
{
    if (utf8.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), int(utf8.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), int(utf8.size()), &w[0], n);
    return w;
}
#endif

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
#ifdef _WIN32
    const std::wstring wpath = toWide(path);
    std::ofstream f(wpath.c_str(), std::ios::binary); // 二进制模式
#else
    std::ofstream f(path, std::ios::binary); // 二进制模式
#endif
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

// 读取一个 4 字节的 RIFF 子块标签（如 "fmt "、"data"、"LIST"）
static void read_tag(std::ifstream& f, char tag[5])
{
    f.read(tag, 4);
    tag[4] = '\0';
}

// 从 WAV 文件里读出音频，统一降为 48k 单声道
// 不同工具导出的 wav 结构不一样（可能带 LIST 等额外块，也可能是立体声），
// 所以这里按 RIFF 规则遍历子块来找 fmt / data，并在读取时把多声道平均成单声道
bool read_wav(const std::string& path, AudioBuffer& out)
{
#ifdef _WIN32
    const std::wstring wpath = toWide(path);
    std::ifstream f(wpath.c_str(), std::ios::binary);
#else
    std::ifstream f(path, std::ios::binary);
#endif
    if (!f) return false;

    // RIFF 头
    char tag[5];
    read_tag(f, tag);   // "RIFF"
    read_u32(f);        // riff_size 丢弃
    read_tag(f, tag);   // "WAVE"

    // 遍历各子块，找到 fmt 和 data（位置不固定，中间可能有 LIST 等块）
    uint16_t channels = 1;          // 声道数，默认按单声道
    uint16_t bits = 16;             // 位深，默认 16
    uint32_t sample_rate = 48000;   // 采样率，默认 48k
    uint32_t data_size = 0;         // 音频数据字节数

    // 循环遍历子块
    while (f)
    {
        read_tag(f, tag);
        const uint32_t size = read_u32(f); // 本块大小

        if (std::string(tag) == "fmt ")
        {
            read_u16(f);                // 编码（1=PCM）丢弃
            channels = read_u16(f);     // 声道数
            sample_rate = read_u32(f);  // 采样率
            read_u32(f);                // 字节率 丢弃
            read_u16(f);                // 块对齐 丢弃
            bits = read_u16(f);         // 位深
            const int consumed = 16;    // 上面读了 16 字节
            if (int(size) > consumed)
                f.ignore(size - consumed); // fmt 块多出来的字节跳过
        }
        else if (std::string(tag) == "data")
        {
            data_size = size;           // 找到音频数据
            break;
        }
        else
        {
            // 其它块（LIST 等）整块跳过；RIFF 规定奇数大小要补 1 字节对齐
            f.ignore(size + (size & 1));
        }
    }

    if (channels < 1) channels = 1;
    if (bits == 0) bits = 16;

    // 读音频数据：每帧有 channels 个采样，读取时求平均降为单声道
    const int bytes_per_sample = bits / 8;
    const int frame_count = int(data_size / (channels * bytes_per_sample));
    out.sample_rate = int(sample_rate);
    out.mono.resize(frame_count);
    for (int i = 0; i < frame_count; ++i)
    {
        int sum = 0;
        for (int ch = 0; ch < channels; ++ch)
        {
            // 按 16bit 有符号读，避免多个通道相加时带出直流偏置
            sum += int16_t(read_u16(f));
        }
        out.mono[i] = sum / (channels * 32767.0f); // 平均 → 单声道 float
    }
    return true;
}

} // namespace xsstv
