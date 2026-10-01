#pragma once
/*
    DF3DS RET1 SDL-shaped compatibility edge.

    This is NOT SDL and does not link SDL.
    It only maps the small SDL-shaped platform API still named by retained
    TFE owners onto libctru or inert/null behavior for the first visual
    runtime bring-up.
*/

#include "df3ds_ctru.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <strings.h>

using Uint8  = std::uint8_t;
using Uint16 = std::uint16_t;
using Uint32 = std::uint32_t;
using Uint64 = std::uint64_t;
using Sint8  = std::int8_t;
using Sint16 = std::int16_t;
using Sint32 = std::int32_t;
using Sint64 = std::int64_t;

using SDL_bool = int;
constexpr SDL_bool SDL_FALSE = 0;
constexpr SDL_bool SDL_TRUE  = 1;

inline Uint64 SDL_GetPerformanceCounter()
{
    return static_cast<Uint64>(osGetTime());
}

inline Uint64 SDL_GetPerformanceFrequency()
{
    return 1000ull;
}

inline Uint32 SDL_GetTicks()
{
    return static_cast<Uint32>(osGetTime());
}

inline Uint64 SDL_GetTicks64()
{
    return static_cast<Uint64>(osGetTime());
}

inline void SDL_Delay(Uint32 ms)
{
    svcSleepThread(static_cast<s64>(ms) * 1000000LL);
}

inline const char* SDL_GetPlatform()
{
    return "Linux";
}

inline const char* SDL_GetError()
{
    return "DF3DS native compatibility edge";
}

inline Uint32 SDL_GetMouseState(int* x, int* y)
{
    if (x) *x = 0;
    if (y) *y = 0;
    return 0;
}

inline Uint32 SDL_GetRelativeMouseState(int* x, int* y)
{
    if (x) *x = 0;
    if (y) *y = 0;
    return 0;
}

// DF3DS RET1 V22: same-width mouse-coordinate ABI bridge for arm-none-eabi.
#ifndef DF3DS_RET1_MOUSE_COORD_ABI_V22
#define DF3DS_RET1_MOUSE_COORD_ABI_V22 1

template <typename CoordT>
inline Uint32 SDL_GetMouseState(CoordT* x, CoordT* y)
{
    static_assert(
        sizeof(CoordT) == sizeof(int),
        "DF3DS mouse-coordinate bridge requires native-int width parity");

    int nativeX = 0;
    int nativeY = 0;

    const Uint32 state = SDL_GetMouseState(
        x ? &nativeX : nullptr,
        y ? &nativeY : nullptr);

    if (x)
        *x = static_cast<CoordT>(nativeX);

    if (y)
        *y = static_cast<CoordT>(nativeY);

    return state;
}

template <typename CoordT>
inline Uint32 SDL_GetRelativeMouseState(CoordT* x, CoordT* y)
{
    static_assert(
        sizeof(CoordT) == sizeof(int),
        "DF3DS relative-mouse bridge requires native-int width parity");

    int nativeX = 0;
    int nativeY = 0;

    const Uint32 state = SDL_GetRelativeMouseState(
        x ? &nativeX : nullptr,
        y ? &nativeY : nullptr);

    if (x)
        *x = static_cast<CoordT>(nativeX);

    if (y)
        *y = static_cast<CoordT>(nativeY);

    return state;
}

#endif


inline int SDL_SetRelativeMouseMode(SDL_bool)
{
    return 0;
}

struct SDL_mutex
{
    LightLock lock;
};

inline SDL_mutex* SDL_CreateMutex()
{
    SDL_mutex* m =
        static_cast<SDL_mutex*>(std::malloc(sizeof(SDL_mutex)));

    if (m)
        LightLock_Init(&m->lock);

    return m;
}

inline void SDL_DestroyMutex(SDL_mutex* m)
{
    std::free(m);
}

inline int SDL_LockMutex(SDL_mutex* m)
{
    if (m)
        LightLock_Lock(&m->lock);

    return 0;
}

inline int SDL_UnlockMutex(SDL_mutex* m)
{
    if (m)
        LightLock_Unlock(&m->lock);

    return 0;
}

using SDL_ThreadFunction = int (*)(void*);

// -------------------------------------------------------------------------
// DF3DS RET1 V82 — real SDL-shaped worker-thread bridge.
//
// TFE's midiPlayer owns its worker lifecycle through SDL_CreateThread /
// SDL_WaitThread. RET1 previously returned a heap token without ever starting
// the entrypoint, which left iMUSE/MIDI timing dead while reporting success.
//
// Use a separate start payload so SDL_DetachThread can safely release the
// public wrapper immediately without racing the worker's fn/data access.
// -------------------------------------------------------------------------
#ifndef DF3DS_RET1_MIDI_THREAD_V82
#define DF3DS_RET1_MIDI_THREAD_V82 1

struct SDL_Thread
{
    Thread handle;
};

struct DF3DS_SDLThreadStart
{
    SDL_ThreadFunction fn;
    void* data;
};

inline void DF3DS_SDLThreadEntry(void* opaque)
{
    DF3DS_SDLThreadStart* start =
        static_cast<DF3DS_SDLThreadStart*>(opaque);

    SDL_ThreadFunction fn = nullptr;
    void* data = nullptr;

    if (start)
    {
        fn = start->fn;
        data = start->data;
        std::free(start);
    }

    const int result = fn ? fn(data) : 0;
    threadExit(result);
}

inline SDL_Thread* SDL_CreateThread(
    SDL_ThreadFunction fn,
    const char*,
    void* data
)
{
    if (!fn)
        return nullptr;

    SDL_Thread* out =
        static_cast<SDL_Thread*>(std::malloc(sizeof(SDL_Thread)));

    DF3DS_SDLThreadStart* start =
        static_cast<DF3DS_SDLThreadStart*>(
            std::malloc(sizeof(DF3DS_SDLThreadStart))
        );

    if (!out || !start)
    {
        std::free(start);
        std::free(out);
        return nullptr;
    }

    out->handle = nullptr;
    start->fn = fn;
    start->data = data;

    // DF3DS_RET1_MIDI_PRIORITY_V83
    // Marathon 3DS proved a lower-priority music worker can be starved by
    // foreground busy-waits. libctru uses LOWER numeric values for HIGHER
    // priority, so run this MIDI-only worker one step above its caller.
    s32 callerPriority = 0x30;
    if (R_FAILED(svcGetThreadPriority(&callerPriority, CUR_THREAD_HANDLE)))
        callerPriority = 0x30;

    if (callerPriority < 0x18)
        callerPriority = 0x18;
    else if (callerPriority > 0x3f)
        callerPriority = 0x3f;

    const s32 workerPriority =
        callerPriority > 0x18 ? callerPriority - 1 : 0x18;

    out->handle = threadCreate(
        DF3DS_SDLThreadEntry,
        start,
        64 * 1024,
        workerPriority,
        -2,
        false
    );
    if (!out->handle)
    {
        std::free(start);
        std::free(out);
        return nullptr;
    }

    return out;
}

inline int DF3DS_SDLJoinThread(SDL_Thread* t)
{
    if (!t)
        return 0;

    int result = 0;

    if (t->handle)
    {
        threadJoin(t->handle, UINT64_MAX);
        result = threadGetExitCode(t->handle);
        threadFree(t->handle);
        t->handle = nullptr;
    }

    std::free(t);
    return result;
}

inline void SDL_WaitThread(SDL_Thread* t, int* status)
{
    const int result = DF3DS_SDLJoinThread(t);

    if (status)
        *status = result;
}

// -------------------------------------------------------------------------
// DF3DS RET1 WaitThread status portability V9.
//
// Preserve the existing V9 behavior: arm-none-eabi spells TFE's fixed-width
// s32 as long int, while SDL's canonical API uses int*. Both are 32-bit.
// -------------------------------------------------------------------------
#ifndef DF3DS_RET1_WAITTHREAD_STATUS_COMPAT_V9
#define DF3DS_RET1_WAITTHREAD_STATUS_COMPAT_V9 1

template <typename StatusT>
inline void SDL_WaitThread(SDL_Thread* t, StatusT* status)
{
    static_assert(
        sizeof(StatusT) == sizeof(int),
        "DF3DS SDL_WaitThread status must remain 32-bit"
    );

    const int result = DF3DS_SDLJoinThread(t);

    if (status)
        *status = static_cast<StatusT>(result);
}

#endif

inline void SDL_DetachThread(SDL_Thread* t)
{
    if (!t)
        return;

    if (t->handle)
    {
        threadDetach(t->handle);
        t->handle = nullptr;
    }

    std::free(t);
}

inline Uint32 SDL_ThreadID()
{
    // Retained compatibility identity; no retained V82 owner relies on a
    // desktop SDL thread-id value.
    return 1;
}

#endif


using SDL_AudioDeviceID = Uint32;
using SDL_AudioCallback = void (*)(void*, Uint8*, int);

struct SDL_AudioSpec
{
    int freq = 0;
    Uint16 format = 0;
    Uint8 channels = 0;
    Uint8 silence = 0;
    Uint16 samples = 0;
    Uint16 padding = 0;
    Uint32 size = 0;
    SDL_AudioCallback callback = nullptr;
    void* userdata = nullptr;
};

constexpr Uint16 AUDIO_F32SYS = 0x8120;
constexpr Uint16 AUDIO_S16SYS = 0x8010;

inline int SDL_GetNumAudioDevices(int)
{
    return 1;
}

inline const char* SDL_GetAudioDeviceName(int, int)
{
    return "DF3DS NDSP";
}

inline const char* SDL_GetCurrentAudioDriver()
{
    return "DF3DS NDSP";
}

// -------------------------------------------------------------------------
// DF3DS RET1 SOUND1 V81
//
// TFE's audio system already owns mixing and asks SDL_OpenAudioDevice for an
// AUDIO_F32SYS stereo callback sink. RET1 used to acknowledge that request but
// discard it. V81 forwards ONLY the audio-device boundary to a native NDSP
// owner in source/df3ds_audio.cpp.
// -------------------------------------------------------------------------
#ifndef DF3DS_RET1_SOUND1_NDSP_V81
#define DF3DS_RET1_SOUND1_NDSP_V81 1

namespace DF3DS_Audio
{
    SDL_AudioDeviceID open(
        const char* name,
        int capture,
        const SDL_AudioSpec* desired,
        SDL_AudioSpec* obtained,
        int allowedChanges
    );

    void pause(SDL_AudioDeviceID device, int pauseOn);
    void close(SDL_AudioDeviceID device);
}

inline SDL_AudioDeviceID SDL_OpenAudioDevice(
    const char* name,
    int capture,
    const SDL_AudioSpec* desired,
    SDL_AudioSpec* obtained,
    int allowedChanges
)
{
    return DF3DS_Audio::open(
        name,
        capture,
        desired,
        obtained,
        allowedChanges
    );
}

inline void SDL_PauseAudioDevice(SDL_AudioDeviceID device, int pauseOn)
{
    DF3DS_Audio::pause(device, pauseOn);
}

inline void SDL_CloseAudioDevice(SDL_AudioDeviceID device)
{
    DF3DS_Audio::close(device);
}
#endif


inline Uint16 SDL_Swap16(Uint16 x)
{
    return static_cast<Uint16>((x >> 8) | (x << 8));
}

inline Uint32 SDL_Swap32(Uint32 x)
{
    return __builtin_bswap32(x);
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
inline Uint16 SDL_SwapLE16(Uint16 x) { return x; }
inline Uint32 SDL_SwapLE32(Uint32 x) { return x; }
inline Uint16 SDL_SwapBE16(Uint16 x) { return SDL_Swap16(x); }
inline Uint32 SDL_SwapBE32(Uint32 x) { return SDL_Swap32(x); }
#else
inline Uint16 SDL_SwapLE16(Uint16 x) { return SDL_Swap16(x); }
inline Uint32 SDL_SwapLE32(Uint32 x) { return SDL_Swap32(x); }
inline Uint16 SDL_SwapBE16(Uint16 x) { return x; }
inline Uint32 SDL_SwapBE32(Uint32 x) { return x; }
#endif

#define SDL_zero(x) std::memset(&(x), 0, sizeof(x))
#define SDL_memcpy std::memcpy
#define SDL_memset std::memset
#define SDL_strlen std::strlen
#define SDL_strcasecmp strcasecmp

// -------------------------------------------------------------------------
// DF3DS RET1 audio-format compatibility V6.
//
// TFE_Audio/audioDevice.cpp explicitly requests SDL's little-endian float
// spelling AUDIO_F32LSB. The existing native RET1 SDL shim already owns
// AUDIO_F32SYS. Nintendo 3DS is little-endian, so these are the same format.
//
// This is compile-surface compatibility only. RET1 audio output remains null;
// no SDL audio backend or NDSP backend is enabled by this definition.
// -------------------------------------------------------------------------
#ifndef DF3DS_RET1_AUDIO_FORMAT_COMPAT_V6
#define DF3DS_RET1_AUDIO_FORMAT_COMPAT_V6 1

#ifndef AUDIO_F32LSB
#define AUDIO_F32LSB AUDIO_F32SYS
#endif

#endif
