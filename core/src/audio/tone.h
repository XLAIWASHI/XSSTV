#pragma once

#include "xsstv/AudioBuffer.h"

namespace xsstv
{
    
void append_tone(AudioBuffer& audio, double freq, double duration_ms);

} // namespace xsstv
