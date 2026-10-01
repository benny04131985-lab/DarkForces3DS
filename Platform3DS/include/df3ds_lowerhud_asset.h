#pragma once
#include <cstddef>
#include <cstdint>

namespace DF3DS_LowerHudAsset
{
    constexpr std::uint32_t TexWidth = 512;
    constexpr std::uint32_t TexHeight = 256;
    constexpr std::size_t TextureBytes =
        TexWidth * TexHeight * sizeof(std::uint16_t);
    const std::uint16_t* tiled();
}
