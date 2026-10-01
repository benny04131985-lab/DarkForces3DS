#include "df3ds_renderbackend.h"
#include "df3ds_ui1.h"
#include "df3ds_ui1_assets.h"

#include <3ds.h>
#include <citro3d.h>

#include <algorithm>
#include <cstring>

#include "vshader_shbin.h"

#ifdef __3DS__
// DF3DS_SRESET1_DUALEYE1_V5_PRESENT
#include <df3ds_dualeye.h>

#ifdef __3DS__
// DF3DS_STEREO2_LOWERHUD1_V4_RENDER
#include <df3ds_lowerhud.h>
#include <df3ds_lowerhud_asset.h>
#include <TFE_DarkForces/GameUI/pda.h> // DF3DS_SHIPFINAL2_PDA_PRESENT
#endif
#endif

#define DF3DS_DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | \
     GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

namespace
{
    constexpr u32 kVirtualWidth  = 320;
    constexpr u32 kVirtualHeight = 200;

    // Citro3D requires power-of-two texture dimensions (8..1024).
    constexpr u32 kTexWidth  = 512;
    constexpr u32 kTexHeight = 256;

    constexpr float kScreenX0 = 8.0f;
    constexpr float kScreenX1 = 392.0f;
    constexpr float kScreenY0 = 0.0f;
    constexpr float kScreenY1 = 240.0f;

    constexpr float kUMax = float(kVirtualWidth) / float(kTexWidth);

    // Direct-Morton texture rows 0..199 hold the live image. With the
    // established vertical source flip and PICA texture orientation,
    // normalized V=0 addresses the padded top of the 256-row texture.
    // Skip the 56 unused rows, then sample through the full live region.
    constexpr float kVMin =
        float(kTexHeight - kVirtualHeight) / float(kTexHeight);
    constexpr float kVMax = 1.0f;

    // Hardware-proven 3DS intra-tile Morton interleave:
    // morton = mortonX[x & 7] + mortonY[y & 7].
    constexpr u8 kMortonX[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };
    constexpr u8 kMortonY[8] = { 0, 2, 8, 10, 32, 34, 40, 42 };

    struct Vertex
    {
        float x, y, z;
        float u, v;
    };

    const Vertex kQuad[6] =
    {
        { kScreenX0, kScreenY0, 0.5f, 0.0f, kVMin },
        { kScreenX1, kScreenY0, 0.5f, kUMax, kVMin },
        { kScreenX1, kScreenY1, 0.5f, kUMax, kVMax },

        { kScreenX1, kScreenY1, 0.5f, kUMax, kVMax },
        { kScreenX0, kScreenY1, 0.5f, 0.0f, kVMax },
        { kScreenX0, kScreenY0, 0.5f, 0.0f, kVMin },
    };


    // DF3DS_RET1_UI1_V8_V6_DUALSCREEN
    constexpr float kUi1SplashUMax = 400.0f / 512.0f;
    constexpr float kUi1SplashVMax = 240.0f / 256.0f;

    // DF3DS_RET1_UI1_FIX1_BOTTOM_UV_LIVE_RANGE
    // The Morton upload is vertically source-flipped. The live 320x200 image
    // therefore occupies normalized V=kVMin..kVMax inside the 512x256 texture.
    // Draw that exact live range across the native 320x240 bottom screen.
    const Vertex kUi1BottomQuad[6] =
    {
        { 0.0f,   0.0f,   0.5f, 0.0f, kVMin },
        { 320.0f, 0.0f,   0.5f, kUMax, kVMin },
        { 320.0f, 240.0f, 0.5f, kUMax, kVMax },
        { 320.0f, 240.0f, 0.5f, kUMax, kVMax },
        { 0.0f,   240.0f, 0.5f, 0.0f, kVMax },
        { 0.0f,   0.0f,   0.5f, 0.0f, kVMin },
    };

    const Vertex kUi1SplashQuad[6] =
    {
        { 0.0f,   0.0f,   0.5f, 0.0f, 0.0f },
        { 400.0f, 0.0f,   0.5f, kUi1SplashUMax, 0.0f },
        { 400.0f, 240.0f, 0.5f, kUi1SplashUMax, kUi1SplashVMax },
        { 400.0f, 240.0f, 0.5f, kUi1SplashUMax, kUi1SplashVMax },
        { 0.0f,   240.0f, 0.5f, 0.0f, kUi1SplashVMax },
        { 0.0f,   0.0f,   0.5f, 0.0f, 0.0f },
    };

    C3D_RenderTarget* gBottom = nullptr;
    C3D_Tex gUi1MenuSplash{};
    C3D_Tex gUi1BootSplash{};
    bool gUi1MenuSplashInit = false;
    bool gUi1BootSplashInit = false;
    void* gUi1BottomVbo = nullptr;
    void* gUi1SplashVbo = nullptr;
    C3D_Mtx gUi1BottomProjection{};

    C3D_RenderTarget* gTop = nullptr;
    C3D_RenderTarget* gTopRight = nullptr;
    C3D_Tex gDualRightTexture{};
    bool gDualRightTextureInit = false;
    C3D_Tex gPdaTopTexture{};
    bool gPdaTopTextureInit = false;
    bool gPdaWasOpen = false;
    C3D_Tex gLowerHudTexture{};
    bool gLowerHudTextureInit = false;
    void* gLowerHudVbo = nullptr;
    constexpr float kLowerHudUMax = 320.0f / 512.0f;
    constexpr float kLowerHudVMax = 240.0f / 256.0f;
    // DF3DS_STEREO2_LOWERHUD1_V4_RENDER: fit complete lower composite to safe area.
    constexpr float kLowerHudFitX0 = 4.0f;
    constexpr float kLowerHudFitY0 = 3.0f;
    constexpr float kLowerHudFitX1 = 316.0f;
    constexpr float kLowerHudFitY1 = 237.0f;
    const Vertex kLowerHudQuad[6] =
    {
        { kLowerHudFitX0, kLowerHudFitY0, 0.5f, 0.0f,          0.0f },
        { kLowerHudFitX1, kLowerHudFitY0, 0.5f, kLowerHudUMax, 0.0f },
        { kLowerHudFitX1, kLowerHudFitY1, 0.5f, kLowerHudUMax, kLowerHudVMax },
        { kLowerHudFitX1, kLowerHudFitY1, 0.5f, kLowerHudUMax, kLowerHudVMax },
        { kLowerHudFitX0, kLowerHudFitY1, 0.5f, 0.0f,          kLowerHudVMax },
        { kLowerHudFitX0, kLowerHudFitY0, 0.5f, 0.0f,          0.0f },
    };
    bool gDualStereoHw = false; // DF3DS_SRESET1_DUALEYE1_V5_PRESENT

    C3D_Tex gTexture{};
    bool gTextureInit = false;

    DVLB_s* gShaderDvlb = nullptr;
    shaderProgram_s gProgram{};
    bool gProgramInit = false;
    int gProjectionLoc = -1;
    C3D_Mtx gProjection{};

    void* gVbo = nullptr;

    u32 gPaletteRaw[256]{};
    u16 gPalette565[256]{};

    bool gBackendReady = false;
    bool gDisplayReady = false;
    bool gFrameOpen = false;

    u32 gPresentedFrames = 0;
    u32 gUploadedFrames = 0;
    size_t gLastUploadBytes = 0;

    inline u16 pack565(u8 r, u8 g, u8 b)
    {
        return static_cast<u16>(
            ((static_cast<u16>(r) >> 3) << 11) |
            ((static_cast<u16>(g) >> 2) << 5) |
            ( static_cast<u16>(b) >> 3)
        );
    }

    inline u32 rgba8888(u8 r, u8 g, u8 b, u8 a = 255)
    {
        // BOOT2's synthetic producer uses R,G,B,A in increasing byte order.
        return static_cast<u32>(r) |
              (static_cast<u32>(g) << 8) |
              (static_cast<u32>(b) << 16) |
              (static_cast<u32>(a) << 24);
    }

    bool initScene()
    {
        gShaderDvlb = DVLB_ParseFile(
            const_cast<u32*>(
                reinterpret_cast<const u32*>(vshader_shbin)
            ),
            vshader_shbin_size
        );
        if (!gShaderDvlb)
            return false;

        shaderProgramInit(&gProgram);
        gProgramInit = true;

        shaderProgramSetVsh(&gProgram, &gShaderDvlb->DVLE[0]);
        C3D_BindProgram(&gProgram);

        gProjectionLoc =
            shaderInstanceGetUniformLocation(gProgram.vertexShader, "projection");

        if (gProjectionLoc < 0)
            return false;

        C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);

        Mtx_OrthoTilt(
            &gProjection,
            0.0f, 400.0f,
            240.0f, 0.0f,
            0.0f, 1.0f,
            true
        );

        Mtx_OrthoTilt(
            &gUi1BottomProjection,
            0.0f, 320.0f,
            240.0f, 0.0f,
            0.0f, 1.0f,
            true
        );

        gVbo = linearAlloc(sizeof(kQuad));
        if (!gVbo)
            return false;

        std::memcpy(gVbo, kQuad, sizeof(kQuad));

        gUi1BottomVbo = linearAlloc(sizeof(kUi1BottomQuad));
        gUi1SplashVbo = linearAlloc(sizeof(kUi1SplashQuad));
        if (!gUi1BottomVbo || !gUi1SplashVbo)
            return false;

        std::memcpy(gUi1BottomVbo, kUi1BottomQuad, sizeof(kUi1BottomQuad));
        std::memcpy(gUi1SplashVbo, kUi1SplashQuad, sizeof(kUi1SplashQuad));

        C3D_BufInfo* bufInfo = C3D_GetBufInfo();
        BufInfo_Init(bufInfo);
        BufInfo_Add(bufInfo, gVbo, sizeof(Vertex), 2, 0x10);

        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(
            env,
            C3D_Both,
            GPU_TEXTURE0,
            GPU_PRIMARY_COLOR,
            GPU_PRIMARY_COLOR
        );
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

        C3D_CullFace(GPU_CULL_NONE);

        if (!C3D_TexInit(&gTexture, kTexWidth, kTexHeight, GPU_RGB565))
            return false;

        gTextureInit = true;

        C3D_TexSetFilter(&gTexture, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&gTexture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

        // Padding is initialized once. Per-frame uploads touch only the live
        // 320x200 region, matching the proven DIRECTTILED approach.
        std::memset(
            gTexture.data,
            0,
            kTexWidth * kTexHeight * sizeof(u16)
        );
        C3D_TexFlush(&gTexture);

        if (!C3D_TexInit(&gDualRightTexture, kTexWidth, kTexHeight, GPU_RGB565))
            return false;
        gDualRightTextureInit = true;
        C3D_TexSetFilter(&gDualRightTexture, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&gDualRightTexture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        std::memset(gDualRightTexture.data, 0, kTexWidth * kTexHeight * sizeof(u16));
        C3D_TexFlush(&gDualRightTexture);

        if (!C3D_TexInit(&gPdaTopTexture, kTexWidth, kTexHeight, GPU_RGB565))
            return false;
        gPdaTopTextureInit = true;
        C3D_TexSetFilter(&gPdaTopTexture, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&gPdaTopTexture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        std::memset(gPdaTopTexture.data, 0, kTexWidth * kTexHeight * sizeof(u16));
        C3D_TexFlush(&gPdaTopTexture);

        if (!C3D_TexInit(
                &gLowerHudTexture,
                DF3DS_LowerHudAsset::TexWidth,
                DF3DS_LowerHudAsset::TexHeight,
                GPU_RGB565))
            return false;
        gLowerHudTextureInit = true;
        C3D_TexSetFilter(&gLowerHudTexture, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&gLowerHudTexture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        std::memcpy(
            gLowerHudTexture.data,
            DF3DS_LowerHudAsset::tiled(),
            DF3DS_LowerHudAsset::TextureBytes);
        C3D_TexFlush(&gLowerHudTexture);

        gLowerHudVbo = linearAlloc(sizeof(kLowerHudQuad));
        if (!gLowerHudVbo)
            return false;
        std::memcpy(gLowerHudVbo, kLowerHudQuad, sizeof(kLowerHudQuad));

        if (!C3D_TexInit(
                &gUi1MenuSplash,
                DF3DS_UI1_Assets::TexWidth,
                DF3DS_UI1_Assets::TexHeight,
                GPU_RGB565))
            return false;
        gUi1MenuSplashInit = true;
        std::memcpy(
            gUi1MenuSplash.data,
            DF3DS_UI1_Assets::menuSplashTiled(),
            DF3DS_UI1_Assets::TextureBytes
        );
        C3D_TexSetFilter(&gUi1MenuSplash, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(
            &gUi1MenuSplash,
            GPU_CLAMP_TO_EDGE,
            GPU_CLAMP_TO_EDGE
        );
        C3D_TexFlush(&gUi1MenuSplash);

        if (!C3D_TexInit(
                &gUi1BootSplash,
                DF3DS_UI1_Assets::TexWidth,
                DF3DS_UI1_Assets::TexHeight,
                GPU_RGB565))
            return false;
        gUi1BootSplashInit = true;
        std::memcpy(
            gUi1BootSplash.data,
            DF3DS_UI1_Assets::bootSplashTiled(),
            DF3DS_UI1_Assets::TextureBytes
        );
        C3D_TexSetFilter(&gUi1BootSplash, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(
            &gUi1BootSplash,
            GPU_CLAMP_TO_EDGE,
            GPU_CLAMP_TO_EDGE
        );
        C3D_TexFlush(&gUi1BootSplash);

        return true;
    }

    
    // DF3DS_SRESET1_DUALEYE1_V5_PRESENT: second indexed texture uses the exact proven Morton/RGB565 path.
    void df3dsDualUploadIndexed(C3D_Tex* tex, const u8* src)
    {
        if (!tex || !tex->data || !src) return;
        u16* dst = static_cast<u16*>(tex->data);
        constexpr u32 tilesPerRow = kTexWidth / 8;
        for (u32 y = 0; y < kVirtualHeight; ++y)
        {
            const u32 sourceY = (kVirtualHeight - 1u) - y;
            const u8* srcRow = src + sourceY * kVirtualWidth;
            const u32 tileY = y >> 3;
            const u32 inY = y & 7u;
            for (u32 x = 0; x < kVirtualWidth; ++x)
            {
                const u32 tileX = x >> 3;
                const u32 tileBase = (tileY * tilesPerRow + tileX) * 64u;
                const u32 morton = static_cast<u32>(kMortonX[x & 7u]) +
                                   static_cast<u32>(kMortonY[inY]);
                dst[tileBase + morton] = gPalette565[srcRow[x]];
            }
        }
        C3D_TexFlush(tex);
    }

    void df3dsDualDrawTexture(C3D_Tex* tex)
    {
        if (!gBackendReady || !gDisplayReady || !tex || !tex->data) return;
        C3D_BindProgram(&gProgram);
        C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);
        C3D_BufInfo* bufInfo = C3D_GetBufInfo();
        BufInfo_Init(bufInfo);
        BufInfo_Add(bufInfo, gVbo, sizeof(Vertex), 2, 0x10);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, gProjectionLoc, &gProjection);
        C3D_TexBind(0, tex);
        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
        C3D_DrawArrays(GPU_TRIANGLES, 0, 6);
    }

void drawVirtualDisplay()
{
    df3dsDualDrawTexture(&gTexture);
}

    void drawUi1Quad(C3D_Tex* tex, void* vbo, const C3D_Mtx* projection)
    {
        if (!tex || !tex->data || !vbo || !projection)
            return;

        C3D_BindProgram(&gProgram);

        C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);

        C3D_BufInfo* bufInfo = C3D_GetBufInfo();
        BufInfo_Init(bufInfo);
        BufInfo_Add(bufInfo, vbo, sizeof(Vertex), 2, 0x10);

        C3D_FVUnifMtx4x4(
            GPU_VERTEX_SHADER,
            gProjectionLoc,
            projection
        );

        C3D_TexBind(0, tex);

        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(
            env,
            C3D_Both,
            GPU_TEXTURE0,
            GPU_PRIMARY_COLOR,
            GPU_PRIMARY_COLOR
        );
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

        C3D_DrawArrays(GPU_TRIANGLES, 0, 6);
    }


    // DF3DS_STEREO2_LOWERHUD1_V4_RENDER: exact supplied artwork plus indexed map/HUD overlays.
    constexpr int kLowerMapDx = 21;
    constexpr int kLowerMapDy = 5;
    constexpr int kLowerMapDw = 278;
    constexpr int kLowerMapDh = 96;
    constexpr int kLowerMapSx = 0;
    constexpr int kLowerMapSy = 44;
    constexpr int kLowerMapSw = 320;
    constexpr int kLowerMapSh = 112;

    constexpr int kLowerHudLDx = 0;
    constexpr int kLowerHudLDy = 169;
    constexpr int kLowerHudLDw = 144;
    constexpr int kLowerHudLDh = 63;
    constexpr int kLowerHudLSx = 0;
    constexpr int kLowerHudLSy = 158;
    constexpr int kLowerHudLSw = 96;
    constexpr int kLowerHudLSh = 42;

    constexpr int kLowerHudRDx = 176;
    constexpr int kLowerHudRDy = 169;
    constexpr int kLowerHudRDw = 144;
    constexpr int kLowerHudRDh = 63;
    constexpr int kLowerHudRSx = 224;
    constexpr int kLowerHudRSy = 158;
    constexpr int kLowerHudRSw = 96;
    constexpr int kLowerHudRSh = 42;

    inline void df3dsLowerWrite(u16* dst, int x, int y, u16 c)
    {
        if (!dst || x < 0 || x >= 320 || y < 0 || y >= 240) return;
        const u32 texY = 239u - static_cast<u32>(y);
        constexpr u32 tilesPerRow = 512u / 8u;
        const u32 tileX = static_cast<u32>(x) >> 3;
        const u32 tileY = texY >> 3;
        const u32 tileBase = (tileY * tilesPerRow + tileX) * 64u;
        const u32 morton = static_cast<u32>(kMortonX[static_cast<u32>(x) & 7u]) +
                           static_cast<u32>(kMortonY[texY & 7u]);
        dst[tileBase + morton] = c;
    }

    void df3dsLowerBlitIndexed(
        u16* dst, const u8* src, const u8* mask,
        int sx, int sy, int sw, int sh,
        int dx, int dy, int dw, int dh,
        bool zeroTransparent)
    {
        if (!dst || !src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
        for (int y = 0; y < dh; ++y)
        {
            const int srcY = sy + (y * sh) / dh;
            if (srcY < 0 || srcY >= 200) continue;
            for (int x = 0; x < dw; ++x)
            {
                const int srcX = sx + (x * sw) / dw;
                if (srcX < 0 || srcX >= 320) continue;
                const int srcOffset = srcY * 320 + srcX;
                if (mask && !mask[srcOffset]) continue;
                const u8 idx = src[srcOffset];
                if (zeroTransparent && idx == 0) continue;
                df3dsLowerWrite(dst, dx + x, dy + y, gPalette565[idx]);
            }
        }
    }


    // DF3DS_SHIP2B_NEARSHIP1: compact 3x5 uppercase label renderer for the passive center display.
    const char* df3dsGlyph3x5(char c)
    {
        switch (c)
        {
            case 'A': return "111101111101101"; case 'C': return "111100100100111";
            case 'D': return "110101101101110"; case 'E': return "111100110100111";
            case 'G': return "111100101101111"; case 'H': return "101101111101101";
            case 'I': return "111010010010111"; case 'K': return "101101110101101";
            case 'L': return "100100100100111"; case 'M': return "101111111101101";
            case 'N': return "101111111111101"; case 'O': return "111101101101111";
            case 'P': return "110101110100100";
            case 'S': return "111100111001111"; case 'T': return "111010010010010";
            case 'V': return "101101101101010"; case 'R': return "110101110101101";
            default: return nullptr;
        }
    }
    void df3dsDrawInventoryLabel(u16* dst, const char* text)
    {
        if (!dst || !text || !*text) return;
        int len=0; for (const char* p=text; *p; ++p) ++len;
        // DF3DS_SHIPFINAL3_HUDLABEL5
        // SHIPFINAL1 hardware put the 2x label just below the green display.
        // Final hardware polish: native 1x glyphs, slightly higher in the green display.
        const int scale = 1;
        const int advance = 4 * scale;
        const int textWidth = len > 0 ? ((len - 1) * advance + 3 * scale) : 0;
        int x = 160 - textWidth / 2;
        const int y = 136;
        for (const char* p=text; *p; ++p, x+=advance)
        {
            if (*p==' ') continue;
            const char* g=df3dsGlyph3x5(*p); if (!g) continue;
            for (int gy=0; gy<5; ++gy) for (int gx=0; gx<3; ++gx) if (g[gy*3+gx]=='1')
                for (int yy=0; yy<scale; ++yy) for (int xx=0; xx<scale; ++xx)
                    df3dsLowerWrite(dst,x+gx*scale+xx,y+gy*scale+yy,0x07e0);
        }
    }

    void df3dsComposeLowerHud()
    {
        if (!gLowerHudTextureInit || !gLowerHudTexture.data) return;
        std::memcpy(
            gLowerHudTexture.data,
            DF3DS_LowerHudAsset::tiled(),
            DF3DS_LowerHudAsset::TextureBytes);

        if (DF3DS_LowerHud::pauseReady())
        {
            u16* dst = static_cast<u16*>(gLowerHudTexture.data);
            df3dsLowerBlitIndexed(dst, DF3DS_LowerHud::pauseFrame(), nullptr,
                0, 0, 320, 200, 0, 0, 320, 240, false);
        }
        else if (DF3DS_LowerHud::ready())
        {
            u16* dst = static_cast<u16*>(gLowerHudTexture.data);
            df3dsLowerBlitIndexed(
                dst, DF3DS_LowerHud::mapFrame(), nullptr,
                kLowerMapSx, kLowerMapSy, kLowerMapSw, kLowerMapSh,
                kLowerMapDx, kLowerMapDy, kLowerMapDw, kLowerMapDh,
                true);
            df3dsLowerBlitIndexed(
                dst, DF3DS_LowerHud::hudFrame(), DF3DS_LowerHud::hudMask(),
                kLowerHudLSx, kLowerHudLSy, kLowerHudLSw, kLowerHudLSh,
                kLowerHudLDx, kLowerHudLDy, kLowerHudLDw, kLowerHudLDh,
                false);
            df3dsLowerBlitIndexed(
                dst, DF3DS_LowerHud::hudFrame(), DF3DS_LowerHud::hudMask(),
                kLowerHudRSx, kLowerHudRSy, kLowerHudRSw, kLowerHudRSh,
                kLowerHudRDx, kLowerHudRDy, kLowerHudRDw, kLowerHudRDh,
                false);
            df3dsDrawInventoryLabel(dst, DF3DS_LowerHud::inventoryLabel());
        }
        C3D_TexFlush(&gLowerHudTexture);
    }

    void df3dsDrawLowerHud()
    {
        if ((!DF3DS_LowerHud::ready() && !DF3DS_LowerHud::pauseReady()) || !gLowerHudTextureInit || !gLowerHudVbo)
            return;
        df3dsComposeLowerHud();
        drawUi1Quad(&gLowerHudTexture, gLowerHudVbo, &gUi1BottomProjection);
    }



    void drawUi1TopSplash(bool boot)
    {
        drawUi1Quad(
            boot ? &gUi1BootSplash : &gUi1MenuSplash,
            gUi1SplashVbo,
            &gProjection
        );
    }

    void drawUi1BottomMenu()
    {
        if (!gDisplayReady)
            return;

        drawUi1Quad(&gTexture, gUi1BottomVbo, &gUi1BottomProjection);
    }

}

namespace TFE_RenderBackend
{
    bool init(const WindowState& state)
    {
        (void)state;

        if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE))
            return false;

        gTop = C3D_RenderTargetCreate(
            240, 400,
            GPU_RB_RGBA8,
            GPU_RB_DEPTH24_STENCIL8
        );

        if (!gTop)
        {
            C3D_Fini();
            return false;
        }

        C3D_RenderTargetSetOutput(
            gTop,
            GFX_TOP,
            GFX_LEFT,
            DF3DS_DISPLAY_TRANSFER_FLAGS
        );

        gBottom = C3D_RenderTargetCreate(
            240, 320,
            GPU_RB_RGBA8,
            GPU_RB_DEPTH24_STENCIL8
        );
        if (!gBottom)
        {
            C3D_Fini();
            gTop = nullptr;
            return false;
        }

        C3D_RenderTargetSetOutput(
            gBottom,
            GFX_BOTTOM,
            GFX_LEFT,
            DF3DS_DISPLAY_TRANSFER_FLAGS
        );

        // DF3DS_SRESET1_DUALEYE1_V5_PRESENT: allocate/link the right eye once, but leave the
        // LCD in ordinary 2D mode.  gfxSet3D() is toggled only after a
        // gameplay frame has produced a valid dual-eye pair.
        gTopRight = C3D_RenderTargetCreate(
            240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        if (!gTopRight)
        {
            C3D_Fini();
            gBottom = nullptr;
            gTop = nullptr;
            return false;
        }
        C3D_RenderTargetSetOutput(
            gTopRight, GFX_TOP, GFX_RIGHT, DF3DS_DISPLAY_TRANSFER_FLAGS);
        gDualStereoHw = false;

        if (!initScene())
        {
            destroy();
            return false;
        }

        gBackendReady = true;
        return true;
    }

    void destroy()
    {
        gBackendReady = false;
        gDisplayReady = false;
        gfxSet3D(false);
        gDualStereoHw = false;
        DF3DS_DualEye::shutdownSummary();
        if (gDualRightTextureInit)
        {
            C3D_TexDelete(&gDualRightTexture);
            gDualRightTextureInit = false;
        }
        if (gPdaTopTextureInit)
        {
            C3D_TexDelete(&gPdaTopTexture);
            gPdaTopTextureInit = false;
            gPdaWasOpen = false;
        }
        if (gLowerHudTextureInit)
        {
            C3D_TexDelete(&gLowerHudTexture);
            gLowerHudTextureInit = false;
        }
        if (gLowerHudVbo)
        {
            linearFree(gLowerHudVbo);
            gLowerHudVbo = nullptr;
        }

        if (gUi1MenuSplashInit)
        {
            C3D_TexDelete(&gUi1MenuSplash);
            gUi1MenuSplashInit = false;
        }
        if (gUi1BootSplashInit)
        {
            C3D_TexDelete(&gUi1BootSplash);
            gUi1BootSplashInit = false;
        }
        if (gUi1BottomVbo)
        {
            linearFree(gUi1BottomVbo);
            gUi1BottomVbo = nullptr;
        }
        if (gUi1SplashVbo)
        {
            linearFree(gUi1SplashVbo);
            gUi1SplashVbo = nullptr;
        }

        if (gTextureInit)
        {
            C3D_TexDelete(&gTexture);
            gTextureInit = false;
        }

        if (gVbo)
        {
            linearFree(gVbo);
            gVbo = nullptr;
        }

        if (gProgramInit)
        {
            shaderProgramFree(&gProgram);
            gProgramInit = false;
        }

        if (gShaderDvlb)
        {
            DVLB_Free(gShaderDvlb);
            gShaderDvlb = nullptr;
        }

        C3D_Fini();
        gBottom = nullptr;
        gTopRight = nullptr;
        gTop = nullptr;
    }

    bool createVirtualDisplay(const VirtualDisplayInfo& vdispInfo)
    {
        (void)vdispInfo;

        gDisplayReady =
            gBackendReady &&
            gTextureInit &&
            gTexture.data != nullptr;

        return gDisplayReady;
    }

    void setPalette(const u32* palette)
    {
        if (!palette)
            return;

        std::memcpy(gPaletteRaw, palette, sizeof(gPaletteRaw));

        for (u32 i = 0; i < 256; ++i)
        {
            const u32 c = palette[i];
            const u8 r = static_cast<u8>( c        & 0xff);
            const u8 g = static_cast<u8>((c >> 8)  & 0xff);
            const u8 b = static_cast<u8>((c >> 16) & 0xff);
            gPalette565[i] = pack565(r, g, b);
        }
    }

    const u32* getPalette()
    {
        return gPaletteRaw;
    }

    void updateVirtualDisplay(const void* buffer, size_t size)
    {
        if (!gDisplayReady || !buffer || !gTexture.data)
            return;

        constexpr size_t expected =
            static_cast<size_t>(kVirtualWidth) *
            static_cast<size_t>(kVirtualHeight);
        if (size < expected)
            return;

        // Preserve CITADEL-V15B ordering: drain/open PICA before mutable texture writes.
        if (!gFrameOpen)
        {
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            gFrameOpen = true;
        }

        // DF3DS_SHIPFINAL2_PDA_PRESENT: freeze the last gameplay RGB565 texture
        // before the first PDA upload, then grayscale that independent copy.
        const bool df3dsPdaOpen = TFE_DarkForces::pda_isOpen();
        if (df3dsPdaOpen && !gPdaWasOpen && gPdaTopTextureInit)
        {
            std::memcpy(gPdaTopTexture.data, gTexture.data,
                kTexWidth * kTexHeight * sizeof(u16));
            u16* px = static_cast<u16*>(gPdaTopTexture.data);
            const u32 count = kTexWidth * kTexHeight;
            for (u32 i = 0; i < count; ++i)
            {
                const u16 c = px[i];
                const u8 r = static_cast<u8>(((c >> 11) & 31u) * 255u / 31u);
                const u8 g = static_cast<u8>(((c >> 5) & 63u) * 255u / 63u);
                const u8 b = static_cast<u8>((c & 31u) * 255u / 31u);
                const u8 y = static_cast<u8>((u32(r)*54u + u32(g)*183u + u32(b)*19u) >> 8);
                px[i] = pack565(y, y, y);
            }
            C3D_TexFlush(&gPdaTopTexture);
        }
        gPdaWasOpen = df3dsPdaOpen;

        const bool df3dsDualUpload =
            DF3DS_UI1::mode() == DF3DS_UI1::Mode::Gameplay &&
            DF3DS_DualEye::ready() && gDualRightTextureInit;
        if (df3dsDualUpload)
        {
            df3dsDualUploadIndexed(&gTexture, DF3DS_DualEye::leftFrame());
            df3dsDualUploadIndexed(&gDualRightTexture, DF3DS_DualEye::rightFrame());
        }
        else
        {
            df3dsDualUploadIndexed(&gTexture, static_cast<const u8*>(buffer));
        }

        gLastUploadBytes = size;
        ++gUploadedFrames;
    }

    void bindVirtualDisplay()
    {
        // Classic framebuffer is texture-backed on 3DS.
    }

    void clearVirtualDisplay(f32* color, bool clearColor)
    {
        (void)color;
        (void)clearColor;
    }

    void unbindRenderTarget()
    {
        // No offscreen GPU target in BOOT2 classic-texture path.
    }

    void swap(bool blitVirtualDisplay)
    {
        if (!gBackendReady || !gTop || !gBottom)
            return;

        if (!gFrameOpen)
        {
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            gFrameOpen = true;
        }

        const DF3DS_UI1::Mode uiMode = DF3DS_UI1::mode();
        const bool stereoFrame =
            blitVirtualDisplay &&
            uiMode == DF3DS_UI1::Mode::Gameplay &&
            DF3DS_DualEye::ready() &&
            gTopRight && gDualRightTextureInit;

        // A ready pair is meaningful only for live Gameplay presentation.
        // Cancel it explicitly on every mono path so frontend/loading frames
        // can never inherit stale eye buffers.
        if (!stereoFrame && DF3DS_DualEye::ready())
            DF3DS_DualEye::cancel();

        if (stereoFrame != gDualStereoHw)
        {
            gfxSet3D(stereoFrame);
            gDualStereoHw = stereoFrame;
        }

        C3D_RenderTargetClear(gTop, C3D_CLEAR_ALL, 0x080808FF, 0);
        C3D_FrameDrawOn(gTop);

        if (uiMode == DF3DS_UI1::Mode::Boot)
            drawUi1TopSplash(true);
        else if (uiMode == DF3DS_UI1::Mode::Frontend)
            drawUi1TopSplash(false);
        else if (blitVirtualDisplay)
        {
            if (TFE_DarkForces::pda_isOpen() && gPdaTopTextureInit && gPdaWasOpen)
                df3dsDualDrawTexture(&gPdaTopTexture);
            else
                drawVirtualDisplay();
        }

        if (stereoFrame)
        {
            C3D_RenderTargetClear(gTopRight, C3D_CLEAR_ALL, 0x080808FF, 0);
            C3D_FrameDrawOn(gTopRight);
            df3dsDualDrawTexture(&gDualRightTexture);
        }

        C3D_RenderTargetClear(gBottom, C3D_CLEAR_ALL, 0x000000FF, 0);
        C3D_FrameDrawOn(gBottom);
        if (uiMode == DF3DS_UI1::Mode::Frontend && blitVirtualDisplay)
            drawUi1BottomMenu();
        else if (uiMode == DF3DS_UI1::Mode::Gameplay &&
                 (DF3DS_LowerHud::ready() || DF3DS_LowerHud::pauseReady()))
            df3dsDrawLowerHud();

        C3D_FrameEnd(0);
        gFrameOpen = false;
        ++gPresentedFrames;
        if (stereoFrame)
            DF3DS_DualEye::onStereoPresented();
    }
}

namespace DF3DS_RenderProbe
{
    void makePalette(u32* p)
    {
        if (!p)
            return;

        for (u32 i = 0; i < 256; ++i)
        {
            const u8 v = static_cast<u8>(i);
            p[i] = rgba8888(v, v, v);
        }

        p[1] = rgba8888(255,   0,   0);
        p[2] = rgba8888(  0, 255,   0);
        p[3] = rgba8888(  0,   0, 255);
        p[4] = rgba8888(255, 255,   0);
        p[5] = rgba8888(255,   0, 255);
        p[6] = rgba8888(  0, 255, 255);
        p[7] = rgba8888(255, 255, 255);
        p[8] = rgba8888( 24,  24,  24);
    }

    void makeIndexedFrame(
        u8* fb,
        u32 width,
        u32 height,
        u32 frameNo
    )
    {
        if (!fb || width == 0 || height == 0)
            return;

        // Quadrants make all orientation failures obvious.
        for (u32 y = 0; y < height; ++y)
        {
            for (u32 x = 0; x < width; ++x)
            {
                const bool right = x >= width / 2;
                const bool bottom = y >= height / 2;

                u8 index = 1; // red
                if (right && !bottom)
                    index = 2; // green
                else if (!right && bottom)
                    index = 3; // blue
                else if (right && bottom)
                    index = 4; // yellow

                fb[y * width + x] = index;
            }
        }

        // White border.
        for (u32 x = 0; x < width; ++x)
        {
            fb[x] = 7;
            fb[(height - 1u) * width + x] = 7;
        }

        for (u32 y = 0; y < height; ++y)
        {
            fb[y * width] = 7;
            fb[y * width + (width - 1u)] = 7;
        }

        // Magenta diagonal.
        const u32 diag = std::min(width, height);
        for (u32 i = 0; i < diag; ++i)
            fb[i * width + i] = 5;

        // Moving cyan marker proves fresh uploads.
        const u32 markerX = (frameNo / 2u) % width;
        for (u32 y = 0; y < height; ++y)
            fb[y * width + markerX] = 6;
    }

    bool backendReady()
    {
        return gBackendReady && gDisplayReady;
    }

    u32 presentedFrames()
    {
        return gPresentedFrames;
    }

    u32 uploadedFrames()
    {
        return gUploadedFrames;
    }

    size_t lastUploadBytes()
    {
        return gLastUploadBytes;
    }
}
