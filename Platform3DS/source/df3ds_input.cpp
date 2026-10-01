#include "df3ds_input.h"
#include "df3ds_ui1.h"
#include "df3ds_lowerhud.h"
#include <TFE_DarkForces/GameUI/escapeMenu.h>
#include <TFE_DarkForces/GameUI/pda.h>
#include <TFE_DarkForces/hud.h>
#include <TFE_RenderBackend/renderBackend.h> // DF3DS_V100_RC2_FINISH1 OEM Y inverse

#define KEY_A CTR_KEY_A
#define KEY_B CTR_KEY_B
#define KEY_SELECT CTR_KEY_SELECT
#define KEY_START CTR_KEY_START
#define KEY_DRIGHT CTR_KEY_DRIGHT
#define KEY_DLEFT CTR_KEY_DLEFT
#define KEY_DUP CTR_KEY_DUP
#define KEY_DDOWN CTR_KEY_DDOWN
#define KEY_RIGHT CTR_KEY_RIGHT
#define KEY_LEFT CTR_KEY_LEFT
#define KEY_UP CTR_KEY_UP
#define KEY_DOWN CTR_KEY_DOWN
#define KEY_R CTR_KEY_R
#define KEY_L CTR_KEY_L
#define KEY_X CTR_KEY_X
#define KEY_Y CTR_KEY_Y
#define KEY_ZL CTR_KEY_ZL
#define KEY_ZR CTR_KEY_ZR
#define KEY_TOUCH CTR_KEY_TOUCH
#define KEY_CSTICK_RIGHT CTR_KEY_CSTICK_RIGHT
#define KEY_CSTICK_LEFT CTR_KEY_CSTICK_LEFT
#define KEY_CSTICK_UP CTR_KEY_CSTICK_UP
#define KEY_CSTICK_DOWN CTR_KEY_CSTICK_DOWN
#define KEY_CPAD_RIGHT CTR_KEY_CPAD_RIGHT
#define KEY_CPAD_LEFT CTR_KEY_CPAD_LEFT
#define KEY_CPAD_UP CTR_KEY_CPAD_UP
#define KEY_CPAD_DOWN CTR_KEY_CPAD_DOWN
#include <3ds.h>
#undef KEY_A
#undef KEY_B
#undef KEY_SELECT
#undef KEY_START
#undef KEY_DRIGHT
#undef KEY_DLEFT
#undef KEY_DUP
#undef KEY_DDOWN
#undef KEY_RIGHT
#undef KEY_LEFT
#undef KEY_UP
#undef KEY_DOWN
#undef KEY_R
#undef KEY_L
#undef KEY_X
#undef KEY_Y
#undef KEY_ZL
#undef KEY_ZR
#undef KEY_TOUCH
#undef KEY_CSTICK_RIGHT
#undef KEY_CSTICK_LEFT
#undef KEY_CSTICK_UP
#undef KEY_CSTICK_DOWN
#undef KEY_CPAD_RIGHT
#undef KEY_CPAD_LEFT
#undef KEY_CPAD_UP
#undef KEY_CPAD_DOWN

#include <TFE_Input/input.h>
#include <TFE_Input/inputMapping.h>
#include <cstdio>

// DF3DS_RET1_BOTTOMCONSOLE_V73V2
extern "C" void df3ds_ret1_bottom_ui_claim();

// DF3DS_RET1_CONTROLS1_V72_V5
namespace
{
    // DF3DS_RET1_DIRECTACTION1_V79_V2
    bool s_v79Forward = false;
    bool s_v79Backward = false;
    bool s_v79TurnLeft = false;
    bool s_v79TurnRight = false;
    bool s_v79Primary = false;
    bool s_v79Secondary = false;
    bool s_v79Jump = false;
    bool s_v79Crouch = false;
    bool s_v79Use = false;

    // DF3DS_SHIP2B_INPUTSAFE1_INVENTORYSEM1
    bool s_ship2bMenu = false;
    bool s_ship2bPda = false; // DF3DS_SHIPFINAL1_SELECT_F1
    bool s_ship2bWpnPrev = false;
    bool s_ship2bWpnNext = false;
    bool s_ship2bInventoryUse = false;
    bool s_ship2bMenuMouseR = false;
    bool s_ship2bQuitRequested = false; // DF3DS_SHIP2B_POLISH1_SHARED_QUIT
    int s_ship2bInventory = 0;

    // Quick inventory: only equipment with existing TFE gameplay semantics.
    // Mission/key inventory remains authoritative in the F1/PDA screen.
    const char* const kShip2bInventoryNames[] =
    {
        "HEADLAMP", "NIGHT VISION", "CLEATS", "GAS MASK"
    };
    constexpr int kShip2bInventoryCount =
        int(sizeof(kShip2bInventoryNames) / sizeof(kShip2bInventoryNames[0]));

    void ship2bCycleInventory(int dir)
    {
        s_ship2bInventory += dir;
        if (s_ship2bInventory < 0) s_ship2bInventory = kShip2bInventoryCount - 1;
        if (s_ship2bInventory >= kShip2bInventoryCount) s_ship2bInventory = 0;
        // DF3DS_SHIP2B_POLISH1_INVENTORYHUD: lower-center is the sole
        // quick-inventory selection display; no upper-screen announcement.
        DF3DS_LowerHud::setInventoryLabel(kShip2bInventoryNames[s_ship2bInventory]);
    }

    __attribute__((noinline))
    void v79v2DriveAction(
        decltype(TFE_Input::IADF_FORWARD) action,
        bool now,
        bool& was)
    {
        if (now)
        {
            if (was)
                TFE_Input::inputMapping_setStateDown(action);
            else
                TFE_Input::inputMapping_setStatePress(action);
        }
        else
        {
            TFE_Input::inputMapping_removeState(action);
        }
        was = now;
    }

    enum LogicalSlot
    {
        LS_UP = 0,
        LS_DOWN,
        LS_LEFT,
        LS_RIGHT,
        LS_ALT,
        LS_PRIMARY,
        LS_SECONDARY,
        LS_JUMP,
        LS_CROUCH,
        LS_USE,
        LS_CONFIRM,
        LS_ESCAPE,
        LS_COUNT
    };

    struct KeyLatch
    {
        KeyboardCode logical;
        bool down;
    };

    KeyLatch s_keys[LS_COUNT] =
    {
        { KEY_UP,        false },
        { KEY_DOWN,      false },
        { KEY_LEFT,      false },
        { KEY_RIGHT,     false },
        { KEY_LALT,   false },
        { KEY_LCTRL,  false },
        { static_cast<KeyboardCode>('z'),     false },
        { static_cast<KeyboardCode>('x'),     false },
        { static_cast<KeyboardCode>('c'),     false },
        { static_cast<KeyboardCode>(' '), false },
        { KEY_RETURN,    false },
        { KEY_ESCAPE,    false },
    };

    int s_virtualMouseX = 160;
    int s_virtualMouseY = 100;

    int clampInt(int v, int lo, int hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    int absInt(int v)
    {
        return v < 0 ? -v : v;
    }

    int cstickToMouse(s16 value)
    {
        const int v = static_cast<int>(value);

        if (absInt(v) <= 14)
            return 0;

        int adjusted =
            v > 0 ? (v - 14) : (v + 14);

        int out = adjusted / 18;

        if (out == 0)
            out = adjusted > 0 ? 1 : -1;

        return out;
    }

    void press(KeyboardCode key)
    {
        TFE_Input::setKeyDown(key, false);
        TFE_Input::setBufferedKey(key);
    }

    void release(KeyboardCode key)
    {
        TFE_Input::setKeyUp(key);
    }

    void drive(LogicalSlot slot, bool now)
    {
        KeyLatch& latch = s_keys[slot];

        if (now && !latch.down)
            press(latch.logical);
        else if (!now && latch.down)
            release(latch.logical);

        latch.down = now;
    }

    // DF3DS_RET1_DIAGKEY1_V73
    struct DiagKeyHit
    {
        KeyboardCode key;
        char text;
        const char* label;
        bool valid;
    };

    bool s_diagReady = false;
    bool s_diagTouchActive = false;
    KeyboardCode s_diagTouchKey = KEY_ESCAPE;

    void diagStatus(const char* label)
    {
 (void)label;
}

    void diagInit()
    {
 // V100 RC1: legacy diagnostic console disabled.
}

    DiagKeyHit diagHit(int x, int y)
    {
        DiagKeyHit hit = { KEY_ESCAPE, 0, "-", false };

        if (x < 0 || x >= 320 || y < 40 || y >= 232)
            return hit;

        // DF3DS_RET1_TOUCHALIGN1_V74
        // Console glyphs are 8 px wide. These centers come from the exact
        // strings drawn by diagInit(), so visible key and touch owner agree.
        auto nearest = [](int px, const int* centers, int count, int radius)
        {
            int best = -1;
            int bestDist = 0x7fffffff;
            for (int i = 0; i < count; ++i)
            {
                const int d = centers[i] > px
                    ? centers[i] - px
                    : px - centers[i];
                if (d < bestDist)
                {
                    bestDist = d;
                    best = i;
                }
            }
            return bestDist <= radius ? best : -1;
        };

        if (y < 72)
        {
            static const int centers[5] = { 20, 68, 124, 176, 228 };
            static const KeyboardCode keys[5] = {
                KEY_ESCAPE, KEY_TAB, KEY_N, KEY_BACKSPACE, KEY_RETURN
            };
            static const char texts[5] = { 0, 0, 'n', 0, 0 };
            static const char* labels[5] = {
                "ESC", "TAB", "NEW/N", "BKSP", "ENTER"
            };
            const int i = nearest(x, centers, 5, 26);
            return i >= 0 ? DiagKeyHit{keys[i], texts[i], labels[i], true} : hit;
        }

        if (y < 104)
        {
            static const int centers[10] = {
                12, 36, 60, 84, 108, 132, 156, 180, 204, 228
            };
            static const KeyboardCode keys[10] = {
                KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
                KEY_6, KEY_7, KEY_8, KEY_9, KEY_0
            };
            static const char texts[10] = {
                '1','2','3','4','5','6','7','8','9','0'
            };
            static const char* labels[10] = {
                "1","2","3","4","5","6","7","8","9","0"
            };
            const int i = nearest(x, centers, 10, 12);
            return i >= 0 ? DiagKeyHit{keys[i], texts[i], labels[i], true} : hit;
        }

        if (y < 136)
        {
            static const int centers[10] = {
                12, 36, 60, 84, 108, 132, 156, 180, 204, 228
            };
            static const KeyboardCode keys[10] = {
                KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T,
                KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P
            };
            static const char texts[10] = {
                'q','w','e','r','t','y','u','i','o','p'
            };
            static const char* labels[10] = {
                "Q","W","E","R","T","Y","U","I","O","P"
            };
            const int i = nearest(x, centers, 10, 12);
            return i >= 0 ? DiagKeyHit{keys[i], texts[i], labels[i], true} : hit;
        }

        if (y < 168)
        {
            static const int centers[9] = {
                20, 52, 84, 116, 148, 180, 212, 244, 276
            };
            static const KeyboardCode keys[9] = {
                KEY_A, KEY_S, KEY_D, KEY_F, KEY_G,
                KEY_H, KEY_J, KEY_K, KEY_L
            };
            static const char texts[9] = {
                'a','s','d','f','g','h','j','k','l'
            };
            static const char* labels[9] = {
                "A","S","D","F","G","H","J","K","L"
            };
            const int i = nearest(x, centers, 9, 16);
            return i >= 0 ? DiagKeyHit{keys[i], texts[i], labels[i], true} : hit;
        }

        if (y < 200)
        {
            static const int centers[7] = { 36, 76, 116, 156, 196, 236, 276 };
            static const KeyboardCode keys[7] = {
                KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M
            };
            static const char texts[7] = { 'z','x','c','v','b','n','m' };
            static const char* labels[7] = { "Z","X","C","V","B","N","M" };
            const int i = nearest(x, centers, 7, 20);
            return i >= 0 ? DiagKeyHit{keys[i], texts[i], labels[i], true} : hit;
        }

        // Printed: "      SPACE       <   v   ^   >"
        if (x >= 16 && x <= 120)
            return { KEY_SPACE, ' ', "SPACE", true };

        static const int centers[4] = { 148, 180, 212, 244 };
        static const KeyboardCode keys[4] = {
            KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT
        };
        static const char* labels[4] = { "LEFT", "DOWN", "UP", "RIGHT" };
        const int i = nearest(x, centers, 4, 16);
        return i >= 0 ? DiagKeyHit{keys[i], 0, labels[i], true} : hit;
    }

    void diagReleaseTouch()
    {
        if (!s_diagTouchActive)
            return;

        release(s_diagTouchKey);
        s_diagTouchActive = false;
    }

    void diagTouch(u32 down, u32 up)
    {
        if (up & CTR_KEY_TOUCH)
            diagReleaseTouch();

        if (!(down & CTR_KEY_TOUCH))
            return;

        touchPosition tp{};
        hidTouchRead(&tp);

        const DiagKeyHit hit = diagHit((int)tp.px, (int)tp.py);
        if (!hit.valid)
            return;

        // Never leave a previous touch key held if a new DOWN is observed.
        diagReleaseTouch();

        press(hit.key);
        s_diagTouchKey = hit.key;
        s_diagTouchActive = true;

        if (hit.text)
        {
            char text[2] = { hit.text, '\0' };
            TFE_Input::setBufferedInput(text);
        }

        diagStatus(hit.label);
    }

}

namespace DF3DS_Input
{

    // DF3DS_RET1_UI1_V8_KEYDOMAIN_SPLIT
    static bool s_df3dsUi1TouchDown = false;
    // DF3DS_RET1_UI1_FIX2_TAP_CNUB_DIRECT
    static bool s_ui1CnubTurnLeft = false;
    static bool s_ui1CnubTurnRight = false;
    static bool s_ui1CnubLookUp = false;
    static bool s_ui1CnubLookDown = false;

    static void df3dsUi1ReleaseTouch()
    {
        if (!s_df3dsUi1TouchDown)
            return;
        TFE_Input::setMouseButtonUp(MBUTTON_LEFT);
        s_df3dsUi1TouchDown = false;
    }

    static void df3dsUi1RouteTouch(u32 held, u32 down, u32 up)
    {
        // libctru KEY_TOUCH == BIT(20). Use the bit value rather than the
        // KEY_TOUCH identifier because TFE intentionally owns its own KEY_*
        // vocabulary in this same translation unit.
        constexpr u32 kTouchBit = (1u << 20);

        // DF3DS_SHIP2B_POLISH1_MOUSE_DOMAIN
        const bool nearshipMouseUi =
            DF3DS_UI1::frontendMode() ||
            TFE_DarkForces::escapeMenu_isOpen() ||
            TFE_DarkForces::pda_isOpen();
        if (!nearshipMouseUi)
        {
            df3dsUi1ReleaseTouch();
            return;
        }

        // FIX2 tap semantics: position only on fresh touch; no held-frame cursor drag.
        if (down & kTouchBit)
        {
            touchPosition tp{};
            hidTouchRead(&tp);
            // DF3DS_RET1_UI1_FIX3_TOUCH_INV56: bottom is native 320x240, RET1/TFE menu space is 320x200.
            // X is pre-inverted for menu_handleMousePosition()'s later 12/10 expansion;
            // Y is reduced from the stretched 240 physical lines to 200 live VFB lines.
            // DF3DS_SHIPFINAL4_TOUCHAFFINE1
            // The lower pause/PDA presentation is a 320x200 source displayed in the
            // 312x234 rectangle at physical (4,3).  Convert the touch into that
            // presented source space, then account for each OEM menu owner's own
            // downstream transform:
            //
            //   PDA menu_handleMousePosition(): x gets an additional 12/10.
            //   Escape escMenu_handleMousePosition(): no 12/10 x expansion.
            //
            // Therefore PDA's injected x is sourceX*5/6, while Escape receives
            // sourceX directly.  Both receive sourceY directly.  Frontend keeps its
            // already-proven legacy mapping and is deliberately untouched.
            const bool escapeMouse = TFE_DarkForces::escapeMenu_isOpen();
            const bool pdaMouse = TFE_DarkForces::pda_isOpen();

            s32 ui1TouchMouseX;
            s32 ui1TouchMouseY;
            if (escapeMouse || pdaMouse)
            {
                const s32 lowerX = clamp(static_cast<s32>(tp.px) - 4, 0, 311);
                const s32 lowerY = clamp(static_cast<s32>(tp.py) - 3, 0, 233);

                // Presented source coordinates:
                // sourceX = lowerX * 320/312 = lowerX * 40/39
                // sourceY = lowerY * 200/234 = lowerY * 100/117
                // DF3DS_V100_RC2_FINISH1: invert the OEM menu owner's later
                // cursorY = mouseY * VFB_HEIGHT / displayInfo.height.
                DisplayInfo df3dsDisplayInfo{};
                TFE_RenderBackend::getDisplayInfo(&df3dsDisplayInfo);
                const s32 df3dsSourceY = (lowerY * 100 + 58) / 117;
                const s32 df3dsDisplayHeight = static_cast<s32>(df3dsDisplayInfo.height);
                // DF3D_FINISH1_POLISH1_YOFFSET: physical touch was uniformly ABOVE rendered cursor.
                // Shift requested cursor up in source-space; retain OEM Y scale.
                constexpr s32 df3dsTouchYOffset = 6;
                const s32 df3dsAdjustedSourceY = df3dsSourceY > df3dsTouchYOffset
                    ? df3dsSourceY - df3dsTouchYOffset : 0;
                ui1TouchMouseY = df3dsDisplayHeight > 0
                    ? (df3dsAdjustedSourceY * df3dsDisplayHeight + 100) / 200
                    : df3dsAdjustedSourceY;
                if (pdaMouse)
                {
                    // menu_handleMousePosition() later applies 12/10 to X:
                    // (40/39)*(10/12) == 100/117.
                    ui1TouchMouseX = (lowerX * 100 + 58) / 117;
                }
                else
                {
                    ui1TouchMouseX = (lowerX * 40 + 19) / 39;
                }
            }
            else
            {
                ui1TouchMouseX = (static_cast<s32>(tp.px) * 5 + 4) / 6;
                ui1TouchMouseY = (static_cast<s32>(tp.py) * 5) / 6;
            }

            s_virtualMouseX = ui1TouchMouseX;
            s_virtualMouseY = ui1TouchMouseY;
            TFE_Input::setMousePos(ui1TouchMouseX, ui1TouchMouseY);
        }

        if ((down & kTouchBit) && !s_df3dsUi1TouchDown)
        {
            TFE_Input::setMouseButtonDown(MBUTTON_LEFT);
            s_df3dsUi1TouchDown = true;
        }

        if ((up & kTouchBit) && s_df3dsUi1TouchDown)
        {
            TFE_Input::setMouseButtonUp(MBUTTON_LEFT);
            s_df3dsUi1TouchDown = false;
        }
    }

    // DF3DS_SHIP2B_POLISH1_SHARED_QUIT

    void requestShipQuit()

    {

        s_ship2bQuitRequested = true;

    }


    bool poll()
    {
        hidScanInput();

        const u32 held = hidKeysHeld();
        const u32 down = hidKeysDown();
        const u32 up = hidKeysUp();

        // DF3DS_SHIPFINAL1: OEM Quit->Yes is the proven 3DS shutdown owner.
        // SELECT becomes native F1/PDA.
        if (s_ship2bQuitRequested)
            return false;

        // DF3DS_RET1_UI1_V8_V6_TOUCH_STRAFE
        df3dsUi1RouteTouch(held, down, up);

        circlePosition cpad{};
        circlePosition cnub{};

        hidCircleRead(&cpad);
        hidCstickRead(&cnub);

        const bool cForward = cpad.dy > 32;
        const bool cBack    = cpad.dy < -32;
        const bool cLeft    = cpad.dx < -32;
        const bool cRight   = cpad.dx > 32;

        // DF3DS_SHIP2B_INPUTSAFE1_INVENTORYSEM1
        const bool ship2bGameplay =
            !DF3DS_UI1::frontendMode() &&
            !TFE_DarkForces::escapeMenu_isOpen() &&
            !TFE_DarkForces::pda_isOpen();

        // Menus keep ordinary arrow navigation; gameplay gets 3DS semantics.
        drive(LS_UP,    !ship2bGameplay && (held & CTR_KEY_DUP)    != 0);
        drive(LS_DOWN,  !ship2bGameplay && (held & CTR_KEY_DDOWN)  != 0);
        drive(LS_LEFT,  !ship2bGameplay && (held & CTR_KEY_DLEFT)  != 0);
        drive(LS_RIGHT, !ship2bGameplay && (held & CTR_KEY_DRIGHT) != 0);
        drive(LS_ALT, false);

        v79v2DriveAction(TFE_Input::IADF_CYCLEWPN_PREV,
            ship2bGameplay && (held & CTR_KEY_DUP) != 0, s_ship2bWpnPrev);
        v79v2DriveAction(TFE_Input::IADF_CYCLEWPN_NEXT,
            ship2bGameplay && (held & CTR_KEY_DDOWN) != 0, s_ship2bWpnNext);

        if (ship2bGameplay && (down & CTR_KEY_DLEFT))
            ship2bCycleInventory(-1);
        if (ship2bGameplay && (down & CTR_KEY_DRIGHT))
            ship2bCycleInventory(1);

        drive(LS_PRIMARY, false);
        drive(LS_SECONDARY, false);
        drive(LS_JUMP, false);
        drive(LS_CROUCH, false);

        const bool aHeld = (held & CTR_KEY_A) != 0;
        drive(LS_USE, false);
        drive(LS_CONFIRM, aHeld);

        // START = native Dark Forces Escape/menu action.
        drive(LS_ESCAPE, false);
        v79v2DriveAction(TFE_Input::IADF_MENU_TOGGLE,
            (held & CTR_KEY_START) != 0, s_ship2bMenu);

        // DF3DS_SHIPFINAL1_SELECT_F1: exact OEM F1 action.
        v79v2DriveAction(TFE_Input::IADF_PDA_TOGGLE,
            (held & CTR_KEY_SELECT) != 0, s_ship2bPda);

        // DF3DS_RET1_DIRECTACTION1_V79_V2: direct gameplay actions; no bind/config lifecycle.
        v79v2DriveAction(TFE_Input::IADF_FORWARD, cForward, s_v79Forward);
        v79v2DriveAction(TFE_Input::IADF_BACKWARD, cBack, s_v79Backward);
        v79v2DriveAction(TFE_Input::IADF_STRAFE_LT, cLeft, s_v79TurnLeft);
        v79v2DriveAction(TFE_Input::IADF_STRAFE_RT, cRight, s_v79TurnRight);
        v79v2DriveAction(TFE_Input::IADF_PRIMARY_FIRE,
            ship2bGameplay && (held & CTR_KEY_R) != 0, s_v79Primary);
        v79v2DriveAction(TFE_Input::IADF_SECONDARY_FIRE,
            (held & CTR_KEY_L) != 0, s_v79Secondary);
        v79v2DriveAction(TFE_Input::IADF_JUMP,
            (held & CTR_KEY_X) != 0, s_v79Jump);
        v79v2DriveAction(TFE_Input::IADF_CROUCH,
            (held & CTR_KEY_B) != 0, s_v79Crouch);
        v79v2DriveAction(TFE_Input::IADF_USE, aHeld, s_v79Use);

        const int baseMouseDx = cstickToMouse(cnub.dx);
        const int baseMouseDy = -cstickToMouse(cnub.dy);

        // DF3DS_SHIP2B_NEARSHIP1: ZL uses the selected quick item through TFE's native action path.
        const bool invUse = ship2bGameplay && (held & CTR_KEY_ZL) != 0;
        const auto invAction = s_ship2bInventory == 0 ? TFE_Input::IADF_HEAD_LAMP_TOGGLE :
                               s_ship2bInventory == 1 ? TFE_Input::IADF_NIGHT_VISION_TOG :
                               s_ship2bInventory == 2 ? TFE_Input::IADF_CLEATS_TOGGLE :
                                                        TFE_Input::IADF_GAS_MASK_TOGGLE;
        v79v2DriveAction(invAction, invUse, s_ship2bInventoryUse);

        // DF3DS_SHIP2B_POLISH1_MOUSE_DOMAIN: C-nub + R share touch's OEM UI domain.
        const bool nearshipMouseUi =
            DF3DS_UI1::frontendMode() ||
            TFE_DarkForces::escapeMenu_isOpen() ||
            TFE_DarkForces::pda_isOpen();

        // DF3DS_V100_CNUBCOMFORT1: menu cursor remains OEM; gameplay yaw +15%, pitch -25%.
        const int mouseDx = nearshipMouseUi ? baseMouseDx : (baseMouseDx * 115) / 100;
        const int mouseDy = nearshipMouseUi ? baseMouseDy : (baseMouseDy * 75) / 100;
        if (nearshipMouseUi && (mouseDx || mouseDy))
        {
            s_virtualMouseX = clampInt(s_virtualMouseX + mouseDx, 0, 319);
            s_virtualMouseY = clampInt(s_virtualMouseY + mouseDy, 0, 199);
            TFE_Input::setMousePos(s_virtualMouseX, s_virtualMouseY);
        }
        const bool nearshipR = nearshipMouseUi && (held & CTR_KEY_R) != 0;
        if (nearshipR && !s_ship2bMenuMouseR) TFE_Input::setMouseButtonDown(MBUTTON_LEFT);
        else if (!nearshipR && s_ship2bMenuMouseR) TFE_Input::setMouseButtonUp(MBUTTON_LEFT);
        s_ship2bMenuMouseR = nearshipR;

        // UI1 FIX2: C-nub uses exact TFE player actions, not relative mouse.
        // cstickToMouse is retained only as the already-proven deadzone/sign filter.
        const bool ui1Gameplay = ship2bGameplay;
        v79v2DriveAction(TFE_Input::IADF_TURN_LT,
            ui1Gameplay && mouseDx < 0, s_ui1CnubTurnLeft);
        v79v2DriveAction(TFE_Input::IADF_TURN_RT,
            ui1Gameplay && mouseDx > 0, s_ui1CnubTurnRight);
        v79v2DriveAction(TFE_Input::IADF_LOOK_UP,
            ui1Gameplay && mouseDy < 0, s_ui1CnubLookUp);
        v79v2DriveAction(TFE_Input::IADF_LOOK_DN,
            ui1Gameplay && mouseDy > 0, s_ui1CnubLookDown);

        return true;
    }

    void releaseAll()
    {
        for (int i = 0; i < LS_COUNT; ++i)
        {
            if (s_keys[i].down)
            {
                release(s_keys[i].logical);
                s_keys[i].down = false;
            }
        }

        diagReleaseTouch();
        df3dsUi1ReleaseTouch();

        TFE_Input::inputMapping_removeState(TFE_Input::IADF_FORWARD);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_BACKWARD);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_STRAFE_LT);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_STRAFE_RT);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_TURN_LT);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_TURN_RT);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_LOOK_UP);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_LOOK_DN);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_PRIMARY_FIRE);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_SECONDARY_FIRE);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_JUMP);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_CROUCH);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_USE);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_MENU_TOGGLE);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_PDA_TOGGLE);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_CYCLEWPN_PREV);
        TFE_Input::inputMapping_removeState(TFE_Input::IADF_CYCLEWPN_NEXT);
        s_ship2bMenu = false;
        s_ship2bPda = false;
        s_ship2bWpnPrev = false;
        s_ship2bWpnNext = false;
        s_ship2bInventoryUse = false;
        if (s_ship2bMenuMouseR) TFE_Input::setMouseButtonUp(MBUTTON_LEFT);
        s_ship2bMenuMouseR = false;
        s_v79Forward = false;
        s_v79Backward = false;
        s_v79TurnLeft = false;
        s_v79TurnRight = false;
        s_ui1CnubTurnLeft = false;
        s_ui1CnubTurnRight = false;
        s_ui1CnubLookUp = false;
        s_ui1CnubLookDown = false;
        s_v79Primary = false;
        s_v79Secondary = false;
        s_v79Jump = false;
        s_v79Crouch = false;
        s_v79Use = false;

        TFE_Input::endFrame();
    }
}
