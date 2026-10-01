#pragma once
#include <cstddef>

namespace DF3DS_UI1
{
    enum class Mode : int
    {
        Boot = 0,
        Gameplay = 1,
        Frontend = 2,
    };

    void showBootSplash();
    void setGameplayMode();
    void setFrontendMode();
    Mode mode();
    bool frontendMode();

    bool promptAgentName(char* out, size_t outSize);
}
