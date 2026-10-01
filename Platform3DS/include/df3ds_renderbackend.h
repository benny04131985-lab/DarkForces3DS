#pragma once

#include <cstddef>
#include <cstdint>

// BOOT2 implements functions using TFE's real public ABI.
#include <TFE_RenderBackend/renderBackend.h>

namespace DF3DS_RenderProbe
{
    void makePalette(u32* palette256);
    void makeIndexedFrame(u8* framebuffer, u32 width, u32 height, u32 frameNo);

    bool backendReady();
    u32 presentedFrames();
    u32 uploadedFrames();
    size_t lastUploadBytes();
}
