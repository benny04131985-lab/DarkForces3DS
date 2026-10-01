#pragma once

/* DF3DS_RET1_DBOPL_BRIDGE_V83
 * 3DS-only opaque adapter between TFE's FM4 register driver and the
 * Project-Citadel-proven DOSBox DBOPL chip emulator.
 */

#include <stdint.h>

namespace DF3DS_DBOPL
{
    struct Chip
    {
        void* impl;
    };

    bool reset(Chip* chip, uint32_t sampleRate);
    void shutdown(Chip* chip);
    void writeReg(Chip* chip, uint16_t reg, uint8_t value);
    void generate(Chip* chip, int16_t stereoFrame[2]);
}
