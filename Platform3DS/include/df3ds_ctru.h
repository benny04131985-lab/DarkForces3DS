#pragma once
/*
    DF3DS libctru KEY_* namespace firewall.

    libctru and TFE both expose unscoped KEY_* enumerators.
    Import libctru with temporary token renames so a translation unit can
    also include TFE_Input/input.h without a global identifier collision.

    Numeric values are unchanged.
*/

#define KEY_A             CTR_KEY_A
#define KEY_B             CTR_KEY_B
#define KEY_SELECT        CTR_KEY_SELECT
#define KEY_START         CTR_KEY_START
#define KEY_DRIGHT        CTR_KEY_DRIGHT
#define KEY_DLEFT         CTR_KEY_DLEFT
#define KEY_DUP           CTR_KEY_DUP
#define KEY_DDOWN         CTR_KEY_DDOWN
#define KEY_R             CTR_KEY_R
#define KEY_L             CTR_KEY_L
#define KEY_X             CTR_KEY_X
#define KEY_Y             CTR_KEY_Y
#define KEY_ZL            CTR_KEY_ZL
#define KEY_ZR            CTR_KEY_ZR
#define KEY_TOUCH         CTR_KEY_TOUCH
#define KEY_CSTICK_RIGHT  CTR_KEY_CSTICK_RIGHT
#define KEY_CSTICK_LEFT   CTR_KEY_CSTICK_LEFT
#define KEY_CSTICK_UP     CTR_KEY_CSTICK_UP
#define KEY_CSTICK_DOWN   CTR_KEY_CSTICK_DOWN
#define KEY_CPAD_RIGHT    CTR_KEY_CPAD_RIGHT
#define KEY_CPAD_LEFT     CTR_KEY_CPAD_LEFT
#define KEY_CPAD_UP       CTR_KEY_CPAD_UP
#define KEY_CPAD_DOWN     CTR_KEY_CPAD_DOWN
#define KEY_RIGHT         CTR_KEY_RIGHT
#define KEY_LEFT          CTR_KEY_LEFT
#define KEY_UP            CTR_KEY_UP
#define KEY_DOWN          CTR_KEY_DOWN

#include <3ds.h>

#undef KEY_A
#undef KEY_B
#undef KEY_SELECT
#undef KEY_START
#undef KEY_DRIGHT
#undef KEY_DLEFT
#undef KEY_DUP
#undef KEY_DDOWN
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
#undef KEY_RIGHT
#undef KEY_LEFT
#undef KEY_UP
#undef KEY_DOWN
