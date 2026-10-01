// DF3DS V36 - RET1 accessibility/caption compatibility owner.
#include <TFE_A11y/accessibility.h>

#include <cctype>
#include <string>

namespace TFE_A11Y
{
    bool cutsceneCaptionsEnabled()
    {
        return false;
    }

    bool gameplayCaptionsEnabled()
    {
        return false;
    }

    void onSoundPlay(char*, CaptionEnv)
    {
    }

    void clearActiveCaptions()
    {
    }

    Vec2f drawCaptions()
    {
        return { 0.0f, 0.0f };
    }

    std::string toLower(std::string input)
    {
        for (char& ch : input)
        {
            ch = static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(ch)
                )
            );
        }
        return input;
    }
}
