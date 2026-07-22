// ----- AI
/* KEY1 按下沿检测：每次按住只产生一次本地启动请求。 */

#ifndef KEY_START_H
#define KEY_START_H

#include <stdbool.h>

typedef struct {
    bool wasPressed;
} KeyStartState;

void KeyStart_Init(KeyStartState *state);
bool KeyStart_OnSample(KeyStartState *state, bool pressed);

#endif /* KEY_START_H */
// ----- AI
