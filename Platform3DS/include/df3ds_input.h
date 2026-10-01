#pragma once

namespace DF3DS_Input
{
    // Poll native HID and produce normalized TFE keyboard state.
    // False means SELECT requested a clean application exit.
    bool poll();

    // Release any logical keys still held before shutdown.
    void releaseAll();
}
