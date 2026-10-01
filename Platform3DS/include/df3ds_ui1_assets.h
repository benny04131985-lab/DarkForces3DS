#pragma once
#include <cstddef>
#include <cstdint>

namespace DF3DS_UI1_Assets
{
    constexpr std::uint32_t Width = 400;
    constexpr std::uint32_t Height = 240;
    constexpr std::uint32_t TexWidth = 512;
    constexpr std::uint32_t TexHeight = 256;
    constexpr size_t TextureBytes = TexWidth * TexHeight * sizeof(std::uint16_t);
    constexpr size_t LinearBytes = Width * Height * sizeof(std::uint16_t);

    const std::uint16_t* menuSplashTiled();
    const std::uint16_t* bootSplashTiled();
    const std::uint16_t* bootSplashLinear();
}
