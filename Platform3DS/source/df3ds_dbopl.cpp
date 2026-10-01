/* DF3DS_RET1_DBOPL_BRIDGE_V83
 *
 * Keep TFE's LucasArts FM4/iMUSE driver as the semantic authority.
 * This file owns only the low-level DOSBox OPL3 emulation leaf.
 */

#include "df3ds_dbopl.h"
#include "df3ds_dbopl_core.h"

#include <new>

namespace DF3DS_DBOPL
{
    static DBOPL::Handler* handler(Chip* chip)
    {
        return chip ? static_cast<DBOPL::Handler*>(chip->impl) : nullptr;
    }

    bool reset(Chip* chip, uint32_t sampleRate)
    {
        if (!chip || sampleRate == 0)
            return false;

        // DBOPL::Handler::Init() owns the core's private InitTables() setup.
        // Stay on the public Handler API instead of reaching into dbopl.cpp internals.
        DBOPL::Handler* h = handler(chip);
        if (!h)
        {
            h = new (std::nothrow) DBOPL::Handler;
            if (!h)
                return false;
            chip->impl = h;
        }
        else
        {
            h->~Handler();
            new (h) DBOPL::Handler;
        }

        h->Init(static_cast<Bitu>(sampleRate));
        return true;
    }

    void shutdown(Chip* chip)
    {
        if (!chip)
            return;

        DBOPL::Handler* h = handler(chip);
        delete h;
        chip->impl = nullptr;
    }

    void writeReg(Chip* chip, uint16_t reg, uint8_t value)
    {
        DBOPL::Handler* h = handler(chip);
        if (!h)
            return;

        h->WriteReg(static_cast<Bit32u>(reg), static_cast<Bit8u>(value));
    }

    void generate(Chip* chip, int16_t stereoFrame[2])
    {
        if (!stereoFrame)
            return;

        DBOPL::Handler* h = handler(chip);
        if (!h)
        {
            stereoFrame[0] = 0;
            stereoFrame[1] = 0;
            return;
        }

        Bitu frames = 1;
        h->GenerateArr(reinterpret_cast<Bit16s*>(stereoFrame), &frames);
    }
}
