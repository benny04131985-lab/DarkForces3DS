#include <TFE_Asset/imageAsset.h>

/*
    RET1 V5:
    Keep classic Dark Forces GOB/BM texture ownership intact.
    Only external PNG/JPG image services are null until the first authentic
    Dark Forces runtime framebuffer is proven on hardware.
*/
namespace TFE_Image
{
    void init()
    {
    }

    void shutdown()
    {
    }

    SDL_Surface* get(const char*)
    {
        return nullptr;
    }

    SDL_Surface* loadFromMemory(const u8*, size_t)
    {
        return nullptr;
    }

    void free(SDL_Surface*)
    {
    }

    void freeAll()
    {
    }

    void writeImage(
        const char*,
        u32,
        u32,
        u32*
    )
    {
    }

    size_t writeImageToMemory(
        u8*,
        u32,
        u32,
        u32,
        u32,
        const u32*
    )
    {
        return 0;
    }

    void readImageFromMemory(
        SDL_Surface** output,
        size_t,
        const u32*
    )
    {
        if (output)
            *output = nullptr;
    }
}
