#pragma once
/*
    DF3DS RET1 V5 — narrow SDL_Surface declaration fallback.

    Used only when an installed SDL2 core surface header is unavailable or
    does not coexist cleanly with the native DF3DS platform shim.

    No SDL library and no SDL2_image library is linked.
*/
#include "SDL.h"

struct SDL_Palette;

struct SDL_PixelFormat
{
    Uint32 format = 0;
    SDL_Palette* palette = nullptr;

    Uint8 BitsPerPixel = 0;
    Uint8 BytesPerPixel = 0;
    Uint8 padding[2] = {0, 0};

    Uint32 Rmask = 0;
    Uint32 Gmask = 0;
    Uint32 Bmask = 0;
    Uint32 Amask = 0;

    Uint8 Rloss = 0;
    Uint8 Gloss = 0;
    Uint8 Bloss = 0;
    Uint8 Aloss = 0;

    Uint8 Rshift = 0;
    Uint8 Gshift = 0;
    Uint8 Bshift = 0;
    Uint8 Ashift = 0;

    int refcount = 0;
    SDL_PixelFormat* next = nullptr;
};

struct SDL_Rect
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct SDL_Surface
{
    Uint32 flags = 0;
    SDL_PixelFormat* format = nullptr;

    int w = 0;
    int h = 0;
    int pitch = 0;

    void* pixels = nullptr;
    void* userdata = nullptr;

    int locked = 0;
    void* list_blitmap = nullptr;

    SDL_Rect clip_rect{};
    void* map = nullptr;

    int refcount = 0;
};
