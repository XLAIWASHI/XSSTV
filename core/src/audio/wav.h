#pragma once

#include <string>
#include "xsstv/AudioBuffer.h"

namespace xsstv
{
    
bool write_wav(const std::string& path, const AudioBuffer& audio);

} // namespace xsstv
