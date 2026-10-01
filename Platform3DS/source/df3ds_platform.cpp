#include "df3ds_platform.h"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

namespace
{
    bool ensureDir(const char* path)
    {
        if (mkdir(path, 0777) == 0)
            return true;
        return errno == EEXIST;
    }
}

namespace DF3DS
{
    bool ensureSdLayout()
    {
        bool ok = true;
        ok = ensureDir("sdmc:/3ds") && ok;
        ok = ensureDir(kAppRoot) && ok;
        ok = ensureDir(kDataDir) && ok;
        ok = ensureDir(kSaveDir) && ok;
        ok = ensureDir(kConfigDir) && ok;
        ok = ensureDir(kLogDir) && ok;
        return ok;
    }

    bool probeDarkGob(long* sizeOut)
    {
        if (sizeOut)
            *sizeOut = -1;

        char path[256];
        std::snprintf(path, sizeof(path), "%s/DARK.GOB", kDataDir);

        FILE* fp = std::fopen(path, "rb");
        if (!fp)
            return false;

        std::fseek(fp, 0, SEEK_END);
        long bytes = std::ftell(fp);
        std::fclose(fp);

        if (sizeOut)
            *sizeOut = bytes;
        return true;
    }

    void writeLayoutReadme()
    {
        char path[256];
        std::snprintf(path, sizeof(path), "%s/DF3DS_SD_LAYOUT.txt", kAppRoot);

        FILE* fp = std::fopen(path, "wb");
        if (!fp)
            return;

        const char* text =
            "DARK FORCES 3DS SD LAYOUT\n"
            "\n"
            "/3ds/DarkForces3DS/\n"
            "  DarkForces3DS.3dsx\n"
            "  data/       <- original legally-owned GOG Dark Forces game data\n"
            "  saves/      <- DF3DS save data\n"
            "  config/     <- DF3DS/TFE configuration\n"
            "  logs/       <- per-run DF3DS hardware logs\n"
            "\n"
            "BOOT1 expects DARK.GOB at:\n"
            "  sdmc:/3ds/DarkForces3DS/data/DARK.GOB\n";

        std::fwrite(text, 1, std::strlen(text), fp);
        std::fclose(fp);
    }
}
