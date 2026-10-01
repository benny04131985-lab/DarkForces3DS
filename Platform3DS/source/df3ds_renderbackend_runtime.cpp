// DF3DS V46 - retained fixed-renderer compatibility owner.
// DF3DS_RET1_RENDER_COMMON_V46
// Owns common RenderBackend ABI plus inert GPU-only compatibility surfaces that
// canonical fixed-path code still references at link time.
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <TFE_Jedi/Renderer/RClassic_GPU/screenDrawGPU.h>
#include <TFE_RenderShared/texturePacker.h>
#include <TFE_PostProcess/postprocess.h>

#include <cstring>

namespace TFE_RenderBackend
{
    static bool s_df3dsVsync = true;

    void enableVsync(bool enable) { s_df3dsVsync = enable; }
    bool getVsyncEnabled() { return s_df3dsVsync; }
    f32 getDisplayRefreshRate() { return 60.0f; }

    void getDisplayInfo(DisplayInfo* displayInfo)
    {
        if (!displayInfo) return;
        *displayInfo = {};
        u32 width = 320, height = 200;
        TFE_Jedi::vfb_getResolution(&width, &height);
        displayInfo->width = width;
        displayInfo->height = height;
    }

    void getCurrentMonitorInfo(MonitorInfo* monitorInfo)
    {
        if (!monitorInfo) return;
        *monitorInfo = {};
    }

    void bloomPostEnable(bool) {}

    RenderTargetHandle createRenderTarget(u32 width, u32 height, bool hasDepthBuffer)
    {
        (void)width; (void)height; (void)hasDepthBuffer;
        return nullptr;
    }
    void freeRenderTarget(RenderTargetHandle handle) { (void)handle; }
    void copyBackbufferToRenderTarget(RenderTargetHandle dst) { (void)dst; }
    const TextureGpu* getRenderTargetTexture(RenderTargetHandle handle)
    {
        (void)handle;
        return nullptr;
    }
    void getRenderTargetDim(RenderTargetHandle handle, u32* width, u32* height)
    {
        (void)handle;
        u32 w = 320, h = 200;
        TFE_Jedi::vfb_getResolution(&w, &h);
        if (width) *width = w;
        if (height) *height = h;
    }

    void captureScreenToMemory(u32* mem)
    {
        if (!mem) return;
        u32 width = 0, height = 0;
        TFE_Jedi::vfb_getResolution(&width, &height);
        const u8* src = TFE_Jedi::vfb_getCpuBuffer();
        const u32* palette = TFE_Jedi::vfb_getPalette();
        const u32 stride = TFE_Jedi::vfb_getStride();
        if (!src || !palette || !width || !height || !stride)
        {
            if (width && height) std::memset(mem, 0, width * height * sizeof(u32));
            return;
        }
        for (u32 y = 0; y < height; ++y)
        {
            const u8* row = src + y * stride;
            u32* dst = mem + y * width;
            for (u32 x = 0; x < width; ++x) dst[x] = palette[row[x]];
        }
    }
}

// Texture-packer calls here maintain only the GPU/true-color atlas path.
namespace TFE_Jedi
{
    void texturepacker_reset() {}
    void texturepacker_setIndexStart(s32 colorIndexStart) { (void)colorIndexStart; }
    void texturepacker_setConversionPalette(s32 index, s32 bpp, const u8* input)
    { (void)index; (void)bpp; (void)input; }

    // Fixed-only Jedi never enables these GPU paths on 3DS. The ABI remains so
    // canonical screenDraw/HUD code can stay source-identical on its CPU path.
    void screenGPU_init() {}
    void screenGPU_destroy() {}
    void screenGPU_beginLines(u32 width, u32 height) { (void)width; (void)height; }
    void screenGPU_endLines() {}
    void screenGPU_beginImageQuads(u32 width, u32 height) { (void)width; (void)height; }
    void screenGPU_endImageQuads() {}
    void screenGPU_addImageQuad(s32 x0, s32 z0, s32 x1, s32 z1, TextureGpu* texture)
    { (void)x0; (void)z0; (void)x1; (void)z1; (void)texture; }
    void screenGPU_addImageQuad(s32 x0, s32 z0, s32 x1, s32 z1, f32 u0, f32 u1, TextureGpu* texture)
    { (void)x0; (void)z0; (void)x1; (void)z1; (void)u0; (void)u1; (void)texture; }
    void screenGPU_beginQuads(u32 width, u32 height) { (void)width; (void)height; }
    void screenGPU_endQuads() {}
    void screenGPU_setHudTextureCallbacks(s32 count, TextureListCallback* callbacks, bool forceAllocation)
    { (void)count; (void)callbacks; (void)forceAllocation; }
    void screenGPU_setIndexedColors(u32 count, const Vec4f* colors)
    { (void)count; (void)colors; }
    void screenGPU_drawPoint(ScreenRect* rect, s32 x, s32 z, u8 color)
    { (void)rect; (void)x; (void)z; (void)color; }
    void screenGPU_drawLine(ScreenRect* rect, s32 x0, s32 z0, s32 x1, s32 z1, u8 color)
    { (void)rect; (void)x0; (void)z0; (void)x1; (void)z1; (void)color; }
    void screenGPU_blitTexture(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, JBool forceTransparency, JBool forceOpaque)
    { (void)texture; (void)rect; (void)x0; (void)y0; (void)forceTransparency; (void)forceOpaque; }
    void screenGPU_blitTextureLit(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, u8 lightLevel, JBool forceTransparency)
    { (void)texture; (void)rect; (void)x0; (void)y0; (void)lightLevel; (void)forceTransparency; }
    void screenGPU_blitTextureScaled(TextureData* texture, DrawRect* rect, fixed16_16 x0, fixed16_16 y0, fixed16_16 xScale, fixed16_16 yScale, u8 lightLevel, JBool forceTransparency)
    { (void)texture; (void)rect; (void)x0; (void)y0; (void)xScale; (void)yScale; (void)lightLevel; (void)forceTransparency; }
    void screenGPU_drawColoredQuad(fixed16_16 x0, fixed16_16 y0, fixed16_16 x1, fixed16_16 y1, u8 color)
    { (void)x0; (void)y0; (void)x1; (void)y1; (void)color; }
    void screenGPU_blitTexture(ScreenImage* texture, DrawRect* rect, s32 x0, s32 y0)
    { (void)texture; (void)rect; (void)x0; (void)y0; }
    void screenGPU_blitTextureScaled(ScreenImage* texture, DrawRect* rect, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale)
    { (void)texture; (void)rect; (void)x0; (void)y0; (void)xScale; (void)yScale; }
    void screenGPU_blitTextureLitScaled(ScreenImage* texture, DrawRect* rect, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale, u8 lightLevel)
    { (void)texture; (void)rect; (void)x0; (void)y0; (void)xScale; (void)yScale; (void)lightLevel; }
    void screenGPU_blitTextureIScale(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, s32 scale)
    { (void)texture; (void)rect; (void)x0; (void)y0; (void)scale; }
}

// Reticle overlays are a GPU postprocess enhancement; fixed 3DS keeps the API
// state machine linkable but intentionally draws no postprocess overlay.
namespace TFE_PostProcess
{
    OverlayID addOverlayTexture(OverlayImage* overlay, f32* tint, f32 screenX, f32 screenY, f32 scale)
    { (void)overlay; (void)tint; (void)screenX; (void)screenY; (void)scale; return 0; }
    void setOverlayImage(OverlayID id, const OverlayImage* image) { (void)id; (void)image; }
    void modifyOverlay(OverlayID id, f32* tint, f32 screenX, f32 screenY, f32 scale)
    { (void)id; (void)tint; (void)screenX; (void)screenY; (void)scale; }
    void enableOverlay(OverlayID id, bool enable) { (void)id; (void)enable; }
    void removeOverlay(OverlayID id) { (void)id; }
}
