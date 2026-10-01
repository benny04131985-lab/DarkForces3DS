#include "df3ds_lowerhud.h"
#include <cstring>

namespace
{
    // DF3DS_STEREO2_LOWERHUD1_V4_MODULE
    alignas(16) std::uint8_t gPreHud[DF3DS_LowerHud::FrameBytes]{};
    alignas(16) std::uint8_t gHud[DF3DS_LowerHud::FrameBytes]{};
    alignas(16) std::uint8_t gHudMask[DF3DS_LowerHud::FrameBytes]{};
    alignas(16) std::uint8_t gMap[DF3DS_LowerHud::FrameBytes]{};

    bool gPreValid = false;
    bool gHudValid = false;
    bool gMapPending = false;
    bool gReady = false;

    void captureRestoreRect(
        std::uint8_t* framebuffer,
        int x0, int y0, int w, int h)
    {
        for (int y = y0; y < y0 + h; ++y)
        {
            if (y < 0 || y >= 200) continue;
            for (int x = x0; x < x0 + w; ++x)
            {
                if (x < 0 || x >= 320) continue;
                const std::size_t p =
                    static_cast<std::size_t>(y) * 320u +
                    static_cast<std::size_t>(x);

                const std::uint8_t before = gPreHud[p];
                const std::uint8_t after = framebuffer[p];

                gHud[p] = after;
                if (after != before)
                    gHudMask[p] = 0xffu;

                // Restore world/weapon pixels, removing only the status HUD.
                framebuffer[p] = before;
            }
        }
    }
}

namespace
{
    // DF3DS_SHIP2B_NEARSHIP1: one 320x200 indexed pause snapshot; no top uploader ownership.
    std::uint8_t gPauseFrame[DF3DS_LowerHud::FrameBytes] = {};
    bool gPauseReady = false;
    char gInventoryLabel[24] = "HEADLAMP";
}

namespace DF3DS_LowerHud
{
    void beginFrame()
    {
        gPreValid = false;
        gHudValid = false;
        gMapPending = false;
        gReady = false;
        std::memset(gHudMask, 0, FrameBytes);
    }

    void beginHudLayer(const std::uint8_t* src, std::size_t size)
    {
        if (!src || size < FrameBytes)
            return;

        std::memcpy(gPreHud, src, FrameBytes);
        gPreValid = true;
    }

    void finishHudLayer(std::uint8_t* framebuffer, std::size_t size)
    {
        if (!framebuffer || size < FrameBytes || !gPreValid)
            return;

        captureRestoreRect(
            framebuffer, HudLeftX, HudLeftY, HudLeftW, HudLeftH);
        captureRestoreRect(
            framebuffer, HudRightX, HudRightY, HudRightW, HudRightH);

        gPreValid = false;
        gHudValid = true;
    }

    std::uint8_t* beginMap()
    {
        std::memset(gMap, 0, FrameBytes);
        gMapPending = true;
        return gMap;
    }

    void commitMap()
    {
        gReady = gHudValid && gMapPending;
        gMapPending = false;
    }

    void cancel()
    {
        gPreValid = false;
        gHudValid = false;
        gMapPending = false;
        gReady = false;
    }

    void setPauseFrame(const std::uint8_t* src, std::size_t size)
    {
        if (!src || size < FrameBytes) { gPauseReady = false; return; }
        std::memcpy(gPauseFrame, src, FrameBytes);
        gPauseReady = true;
    }
    void clearPauseFrame() { gPauseReady = false; }
    bool pauseReady() { return gPauseReady; }
    const std::uint8_t* pauseFrame() { return gPauseFrame; }
    void setInventoryLabel(const char* text)
    {
        if (!text) text = "";
        std::size_t i = 0;
        for (; i + 1 < sizeof(gInventoryLabel) && text[i]; ++i) gInventoryLabel[i] = text[i];
        gInventoryLabel[i] = 0;
    }
    const char* inventoryLabel() { return gInventoryLabel; }

    bool ready() { return gReady; }
    const std::uint8_t* hudFrame() { return gHud; }
    const std::uint8_t* hudMask() { return gHudMask; }
    const std::uint8_t* mapFrame() { return gMap; }
}
