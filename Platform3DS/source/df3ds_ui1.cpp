#include "df3ds_ui1.h"
#include "df3ds_ui1_assets.h"

#include <3ds.h>

#include <algorithm>
#include <cstring>

namespace
{
    __attribute__((used))
    static const char kUi1Identity[] = "DF3DS_RET1_UI1_V8_V6_ACTIVE";

    DF3DS_UI1::Mode gMode = DF3DS_UI1::Mode::Boot;
    inline u8 expandR(u16 c) { return static_cast<u8>(((c >> 11) & 31u) << 3); }
    inline u8 expandG(u16 c) { return static_cast<u8>(((c >> 5) & 63u) << 2); }
    inline u8 expandB(u16 c) { return static_cast<u8>((c & 31u) << 3); }

    void drawTopLinear(const u16* pixels)
    {
        if (!pixels)
            return;

        u16 width = 0;
        u16 height = 0;
        u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height);
        if (!fb)
            return;

        // libctru RGB8 framebuffers are rotated in memory.  Top logical
        // (x,y) maps to physical x*240 + (239-y), BGR bytes.
        for (u32 y = 0; y < 240; ++y)
        {
            for (u32 x = 0; x < 400; ++x)
            {
                const u16 c = pixels[y * 400u + x];
                const size_t o = 3u * (x * 240u + (239u - y));
                fb[o + 0] = expandB(c);
                fb[o + 1] = expandG(c);
                fb[o + 2] = expandR(c);
            }
        }
        GSPGPU_FlushDataCache(fb, 400u * 240u * 3u);
    }

    void clearBottomNative()
    {
        u16 width = 0;
        u16 height = 0;
        u8* fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &width, &height);
        if (!fb)
            return;

        std::memset(fb, 0, 320u * 240u * 3u);
        GSPGPU_FlushDataCache(fb, 320u * 240u * 3u);
    }
}

namespace DF3DS_UI1
{
    void showBootSplash()
    {
        gMode = Mode::Boot;

        // Paint both native double buffers before C3D takes ownership.  The
        // old console is deliberately not initialized, so subsequent printf
        // diagnostics remain in the file log rather than scrolling on screen.
        for (int i = 0; i < 2; ++i)
        {
            drawTopLinear(DF3DS_UI1_Assets::bootSplashLinear());
            clearBottomNative();
            gfxFlushBuffers();
            gfxSwapBuffersGpu();
            gspWaitForVBlank();
        }
    }

    void setGameplayMode()
    {
        // Do not release here: loopGame establishes a default gameplay mode
        // before it dispatches Agent/Briefing, and releasing at this point
        // would destroy a stylus-down edge before the OEM menu consumes it.
        // routeTouch() owns release once a real non-frontend frame is observed.
        gMode = Mode::Gameplay;
    }

    void setFrontendMode()
    {
        gMode = Mode::Frontend;
    }

    Mode mode()
    {
        return gMode;
    }

    bool frontendMode()
    {
        return gMode == Mode::Frontend;
    }

    bool promptAgentName(char* out, size_t outSize)
    {
        if (!out || outSize < 2)
            return false;

        SwkbdState keyboard{};
        swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, 16);

        char text[17] = {};
        const SwkbdButton button = swkbdInputText(&keyboard, text, sizeof(text));
        if (button != SWKBD_BUTTON_CONFIRM || !text[0])
            return false;

        std::strncpy(out, text, outSize - 1);
        out[outSize - 1] = '\0';
        return true;
    }
}
