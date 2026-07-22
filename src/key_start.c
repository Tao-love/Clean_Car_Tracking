// ----- AI
/* KEY1 按下沿检测：不访问硬件，便于主循环和主机测试复用。 */

#include <stdbool.h>

#include "key_start.h"

void KeyStart_Init(KeyStartState *state)
{
    if (state != 0) {
        state->wasPressed = false;
    }
}

bool KeyStart_OnSample(KeyStartState *state, bool pressed)
{
    bool startRequested;

    if (state == 0) {
        return false;
    }
    startRequested = pressed && !state->wasPressed;
    state->wasPressed = pressed;
    return startRequested;
}
// ----- AI
