// DF3DS V36 - native RET1 FrontEnd compatibility owner.
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_Settings/settings.h>
#include <TFE_System/system.h>
#include <TFE_Input/input.h>

#include <string>

namespace TFE_FrontEndUI
{
    static AppState s_ret1State = APP_STATE_GAME;
    static AppState s_ret1MenuReturn = APP_STATE_GAME;

    void exitToMenu()
    {
        s_ret1State = APP_STATE_EXIT_TO_MENU;
        TFE_Input::enableRelativeMode(false);
        TFE_System::postQuitMessage();
    }

    void setState(AppState state)
    {
        s_ret1State = state;

        if (
            state == APP_STATE_QUIT ||
            state == APP_STATE_EXIT_TO_MENU
        )
        {
            TFE_System::postQuitMessage();
        }
    }

    void setMenuReturnState(AppState state)
    {
        s_ret1MenuReturn = state;
        (void)s_ret1MenuReturn;
    }

    bool toggleConsole()
    {
        return false;
    }

    bool isConsoleOpen()
    {
        return false;
    }

    void logToConsole(const char* str)
    {
        TFE_Console::addToHistory(str);
    }

    int getRecordFramerate()
    {
        static const int rates[] = {
            30, 60, 90, 120, 240
        };

        TFE_Settings_Game* settings =
            TFE_Settings::getGameSettings();

        s32 index = settings
            ? settings->df_recordFrameRate
            : 0;

        if (index < 0) index = 0;
        if (index > 4) index = 4;

        return rates[index];
    }

    std::string getPlaybackFramerate()
    {
        static const char* const rates[] = {
            "Original",
            "30",
            "60",
            "90",
            "120",
            "240",
            "Unlimited"
        };

        TFE_Settings_Game* settings =
            TFE_Settings::getGameSettings();

        s32 index = settings
            ? settings->df_playbackFrameRate
            : 0;

        if (index < 0) index = 0;
        if (index > 6) index = 6;

        return std::string(rates[index]);
    }

    bool toggleEnhancements()
    {
        return false;
    }
}
