#pragma once
#include <3ds.h>

namespace DF3DS
{
    static constexpr const char* kAppRoot   = "sdmc:/3ds/DarkForces3DS";
    static constexpr const char* kDataDir   = "sdmc:/3ds/DarkForces3DS/data";
    static constexpr const char* kSaveDir   = "sdmc:/3ds/DarkForces3DS/saves";
    static constexpr const char* kConfigDir = "sdmc:/3ds/DarkForces3DS/config";
    static constexpr const char* kLogDir    = "sdmc:/3ds/DarkForces3DS/logs";

    struct BootState
    {
        bool isNew3DS = false;
        bool speedupRequested = false;
        bool darkGobPresent = false;
        long darkGobBytes = -1;
        bool logOpen = false;
    };

    bool ensureSdLayout();
    bool probeDarkGob(long* sizeOut);
    void writeLayoutReadme();
}
