#pragma once

#include "xsstv/AudioBuffer.h"

namespace xsstv
{
namespace sstv
{
namespace robot36
{
    
// 把VIS头追加到audio末尾
void encode_vis(AudioBuffer& audio);

} // namespace robot36
} // namespace sstv
} // namespace xsstv
