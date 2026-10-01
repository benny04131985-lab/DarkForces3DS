#pragma once
#include <cstddef>
#include <cstdint>

namespace DF3DS_LowerHud
{
    constexpr std::size_t FrameBytes = 320u * 200u;

    constexpr int HudLeftX = 0;
    constexpr int HudLeftY = 158;
    constexpr int HudLeftW = 96;
    constexpr int HudLeftH = 42;
    constexpr int HudRightX = 224;
    constexpr int HudRightY = 158;
    constexpr int HudRightW = 96;
    constexpr int HudRightH = 42;

    void beginFrame();
    void beginHudLayer(const std::uint8_t* src, std::size_t size);
    void finishHudLayer(std::uint8_t* framebuffer, std::size_t size);

    std::uint8_t* beginMap();
    void commitMap();
    void cancel();

    // DF3DS_SHIP2B_NEARSHIP1: pause/menu frame and passive quick-inventory label.
    void setPauseFrame(const std::uint8_t* src, std::size_t size);
    void clearPauseFrame();
    bool pauseReady();
    const std::uint8_t* pauseFrame();
    void setInventoryLabel(const char* text);
    const char* inventoryLabel();

    bool ready();
    const std::uint8_t* hudFrame();
    const std::uint8_t* hudMask();
    const std::uint8_t* mapFrame();
}
