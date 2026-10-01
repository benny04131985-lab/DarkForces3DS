#pragma once
// DF3DS_SRESET1_DUALEYE1_V5_MODULE
#include <cstddef>
#include <cstdint>

// DF3DS_SRESET1_DUALEYE1_V13_MODULE
namespace DF3DS_DualEye
{
    constexpr std::size_t FrameBytes = 320u * 200u;

    void beginFrame();
    bool armLeft();
    void finishLeftWorld();
    bool leftArmed();
    std::uint8_t* beginRightFromLeft(const void* leftFrame, std::size_t size);
    void commitRight();
    void cancel();

    int eyeSign();
    std::int32_t eyeOffsetFixed();
    // DF3DS_STEREO2_DIRECTEYE1_MODULE: explicit magnitude; sign is passed by caller.
    std::int32_t halfSeparationFixed();
    void noteCameraInput(
        int sign,
        std::int32_t baseX,
        std::int32_t baseZ,
        std::int32_t shiftedX,
        std::int32_t shiftedZ);
    bool ready();
    const std::uint8_t* leftFrame();
    const std::uint8_t* rightFrame();

    void onStereoPresented();
    void shutdownSummary();
}
