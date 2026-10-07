#pragma once

#include <string>

#include "xsstv/AudioBuffer.h"

namespace xsstv
{

// 把音频写成 16bit / 单声道 PCM WAV
bool write_wav(const std::string& path, const AudioBuffer& audio);

} // namespace xsstv
