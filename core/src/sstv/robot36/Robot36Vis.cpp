#include "Robot36Vis.h"
#include "Robot36Timing.h"
#include "audio/tone.h"

namespace xsstv
{
namespace sstv
{
namespace robot36
{

void eencode_vis(AudioBuffer& audio)
{
    // 引导音 300ms @ 1900 Hz
    append_tone(audio, FREQ_CENTER, TIME_LEADER);

    // 中断 10ms @ 1200 Hz
    append_tone(audio, FREQ_SYNC, TIME_BREAK);

    // 引导音 300ms @ 1900 Hz
    append_tone(audio, FREQ_CENTER, TIME_LEADER);

    // VIS start bit 30ms @ 1200Hz
    append_tone(audio, FREQ_SYNC, TIME_VIS_START);

    // 7 个比特， LSB 优先
    int vis = VIS_CODE;
    int ones = 0;
    for (int i = 0; i < 7; ++i)
    {
        int bit = (vis >> i) & 1;
        if (bit == 1)
        {
            append_tone(audio, FREQ_VIS_1, TIME_VIS_BIT);
            ones++;
        }
        else
        {
            append_tone(audio, FREQ_VIS_0, TIME_VIS_BIT);
        }
    }

    // 偶校验位
    if (ones % 2 != 0)
    {
        append_tone(audio, FREQ_VIS_1, TIME_VIS_BIT);
    }
    else
    {
        append_tone(audio, FREQ_VIS_0, TIME_VIS_BIT);
    }

    // VIS stop bit 30ms @ 1200 Hz
    append_tone(audio, FREQ_SYNC, TIME_VIS_STOP);
}

} // namespace robot36
} // namespace sstv
} // namespace xsstv
