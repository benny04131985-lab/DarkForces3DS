#include <3ds.h>
#include <SDL.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace DF3DS_Audio
{
namespace
{
    constexpr int kChannel = 0;
    constexpr int kBufferCount = 4;
    constexpr u32 kMinFrames = 64;
    constexpr u32 kMaxFrames = 4096;
    constexpr u32 kThreadStack = 32 * 1024;
    constexpr int kThreadPriority = 0x30;

    std::atomic<bool> s_running{false};
    std::atomic<bool> s_paused{true};

    Thread s_thread = nullptr;
    SDL_AudioCallback s_callback = nullptr;
    void* s_userdata = nullptr;

    ndspWaveBuf s_wave[kBufferCount]{};
    s16* s_pcm = nullptr;
    float* s_mix = nullptr;

    u32 s_frames = 0;
    u32 s_freq = 0;
    u32 s_channels = 0;
    bool s_ndspReady = false;
    bool s_open = false;

    inline s16 floatToPcm16(float v)
    {
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;

        if (v >= 0.0f)
            return static_cast<s16>(v * 32767.0f);

        return static_cast<s16>(v * 32768.0f);
    }

    void fillAndQueue(int index)
    {
        const u32 scalarCount = s_frames * s_channels;
        const u32 floatBytes = scalarCount * sizeof(float);
        s16* dst = s_pcm + static_cast<u32>(index) * scalarCount;

        std::memset(s_mix, 0, floatBytes);

        if (s_callback)
        {
            s_callback(
                s_userdata,
                reinterpret_cast<Uint8*>(s_mix),
                static_cast<int>(floatBytes)
            );
        }

        for (u32 i = 0; i < scalarCount; ++i)
            dst[i] = floatToPcm16(s_mix[i]);

        DSP_FlushDataCache(dst, scalarCount * sizeof(s16));

        std::memset(&s_wave[index], 0, sizeof(s_wave[index]));
        s_wave[index].data_pcm16 = dst;
        s_wave[index].nsamples = s_frames;

        ndspChnWaveBufAdd(kChannel, &s_wave[index]);
    }

    void audioThread(void*)
    {
        while (s_running.load(std::memory_order_acquire))
        {
            if (s_paused.load(std::memory_order_acquire))
            {
                svcSleepThread(1000000LL);
                continue;
            }

            bool queued = false;

            for (int i = 0; i < kBufferCount; ++i)
            {
                const u8 status = s_wave[i].status;

                if (
                    status == NDSP_WBUF_FREE ||
                    status == NDSP_WBUF_DONE
                )
                {
                    fillAndQueue(i);
                    queued = true;
                }
            }

            // A short sleep keeps refill latency low without busy-spinning.
            svcSleepThread(queued ? 500000LL : 1000000LL);
        }
    }

    void releaseBuffers()
    {
        if (s_pcm)
        {
            linearFree(s_pcm);
            s_pcm = nullptr;
        }

        if (s_mix)
        {
            std::free(s_mix);
            s_mix = nullptr;
        }

        std::memset(s_wave, 0, sizeof(s_wave));
        s_frames = 0;
        s_freq = 0;
        s_channels = 0;
    }

    void stopThread()
    {
        if (!s_thread)
            return;

        s_running.store(false, std::memory_order_release);
        threadJoin(s_thread, UINT64_MAX);
        threadFree(s_thread);
        s_thread = nullptr;
    }

    void shutdownDevice()
    {
        stopThread();

        if (s_ndspReady)
        {
            ndspChnSetPaused(kChannel, true);
            ndspChnWaveBufClear(kChannel);
            ndspChnReset(kChannel);
            ndspExit();
            s_ndspReady = false;
        }

        releaseBuffers();

        s_callback = nullptr;
        s_userdata = nullptr;
        s_paused.store(true, std::memory_order_release);
        s_open = false;
    }
}

SDL_AudioDeviceID open(
    const char*,
    int,
    const SDL_AudioSpec* desired,
    SDL_AudioSpec* obtained,
    int
)
{
    if (s_open || !desired || !desired->callback)
        return 0;

    if (
        desired->format != AUDIO_F32SYS ||
        desired->channels != 2 ||
        desired->freq < 8000 ||
        desired->freq > 48000
    )
    {
        return 0;
    }

    const u32 requestedFrames =
        desired->samples ? static_cast<u32>(desired->samples) : 512u;

    if (requestedFrames < kMinFrames || requestedFrames > kMaxFrames)
        return 0;

    s_frames = requestedFrames;
    s_freq = static_cast<u32>(desired->freq);
    s_channels = static_cast<u32>(desired->channels);
    s_callback = desired->callback;
    s_userdata = desired->userdata;

    const u32 scalarCount = s_frames * s_channels;
    const u32 pcmScalarCount = scalarCount * kBufferCount;

    s_pcm = static_cast<s16*>(
        linearAlloc(pcmScalarCount * sizeof(s16))
    );

    s_mix = static_cast<float*>(
        std::malloc(scalarCount * sizeof(float))
    );

    if (!s_pcm || !s_mix)
    {
        shutdownDevice();
        return 0;
    }

    std::memset(s_pcm, 0, pcmScalarCount * sizeof(s16));
    std::memset(s_mix, 0, scalarCount * sizeof(float));
    std::memset(s_wave, 0, sizeof(s_wave));

    const Result ndspResult = ndspInit();
    if (R_FAILED(ndspResult))
    {
        shutdownDevice();
        return 0;
    }

    s_ndspReady = true;

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(kChannel);
    ndspChnSetInterp(kChannel, NDSP_INTERP_LINEAR);
    ndspChnSetRate(kChannel, static_cast<float>(s_freq));
    ndspChnSetFormat(kChannel, NDSP_FORMAT_STEREO_PCM16);
    ndspChnSetPaused(kChannel, true);

    s_paused.store(true, std::memory_order_release);
    s_running.store(true, std::memory_order_release);

    s_thread = threadCreate(
        audioThread,
        nullptr,
        kThreadStack,
        kThreadPriority,
        -2,
        false
    );

    if (!s_thread)
    {
        s_running.store(false, std::memory_order_release);
        shutdownDevice();
        return 0;
    }

    s_open = true;

    if (obtained)
        *obtained = *desired;

    return 1;
}

void pause(SDL_AudioDeviceID device, int pauseOn)
{
    if (!s_open || device != 1 || !s_ndspReady)
        return;

    const bool paused = pauseOn != 0;
    s_paused.store(paused, std::memory_order_release);
    ndspChnSetPaused(kChannel, paused);
}

void close(SDL_AudioDeviceID device)
{
    if (!s_open || device != 1)
        return;

    shutdownDevice();
}
}
