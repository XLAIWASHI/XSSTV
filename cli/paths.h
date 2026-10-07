#pragma once

#include <filesystem>

namespace paths
{
    // inline 头文件定义函数时需要考虑的关键字 .h 会被多个 .cpp include，
    // 如果头文件里直接写了函数实现，链接器会看到好几份同名函数 → 报"重定义"错误。
    // inline 就是告诉编译器："允许这个函数在多个文件里各出现一份，最后合并成一个
    // std::filesystem::path C++17 的“路径”类型，能用 / 拼接路径、跨平台
    inline std::filesystem::path root()   { return XSSTV_ROOT; }
    inline std::filesystem::path assets() { return root() / "assets"; }
    inline std::filesystem::path temp()   { return root() / "temp"; }
    inline std::filesystem::path output() { return root() / "output"; }
} // namespace paths