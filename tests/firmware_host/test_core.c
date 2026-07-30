/* 固定参数、速度 PI、循线 PD 与 KEY1 边沿的纯 C 契约。 */

#include <stdbool.h>
#include <stdint.h>

#include "control_params.h"
#include "control_types.h"
#include "key_start.h"
#include "line_control.h"
#include "speed_pi.h"

static volatile int gTestResult;

void (*const interruptVectors[])(void)
    __attribute__((used, section(".intvecs"))) = {0};

int main(void);

void _c_int00(void)
{
    gTestResult = main();
    for (;;) {
    }
}

static int Test_ControlParams(void)
{
    const ControlParams *params = ControlParams_Get();

    if ((params == 0) ||
        (params->speedKpLeftQ16 != (12L * Q16_ONE)) ||
        (params->speedKiLeftQ16 != 0) ||
        (params->speedFeedforwardLeftQ16 != (7L * Q16_ONE)) ||
        (params->speedKpRightQ16 != (12L * Q16_ONE)) ||
        (params->speedKiRightQ16 != 0) ||
        (params->speedFeedforwardRightQ16 != (6L * Q16_ONE)) ||
        (params->lineKpQ16 != (Q16_ONE / 70)) ||
        (params->lineKdQ16 != (Q16_ONE / 320)) ||
        (params->derivativeAlphaQ16 != (5L * Q16_ONE / 8)) ||
        (params->speedIntegralLimit != 10000) ||
        (params->baseSpeed != 5) ||
        (params->maxTargetSpeed != 67) ||
        (params->maxDeltaSpeed != 67) ||
        (params->maxPwm != 300) ||
        (params->derivativeLimit != 2333) ||
        (params->maxPwm > 400)) {
        return 1;
    }
    return 0;
}

static int Test_SpeedPI(void)
{
    SpeedPIState state = {0};
    SpeedPIOutput output = SpeedPI_Step(
        &state, 10, 5, Q16_ONE, 0, 0, 100, 100);

    if ((output.pwm != 5) || output.saturated || (state.integral != 5)) {
        return 10;
    }
    SpeedPI_Reset(&state);
    output = SpeedPI_Step(
        &state, 10, 0, 100L * Q16_ONE, 0, 0, 100, 100);
    if ((output.pwm != 100) || !output.saturated || (state.integral != 0)) {
        return 11;
    }
    state.integral = 25;
    output = SpeedPI_Step(
        &state, 0, 5, Q16_ONE, Q16_ONE, 0, 100, 100);
    if ((output.pwm != 0) || output.saturated || (state.integral != 0)) {
        return 12;
    }
    return 0;
}

static int Test_LineControl(void)
{
    LineControlState state = {0, 0, 0, false};
    LineSensorSample sample = {0x18U, 2U, true, false, 100};
    ControlParams params = {0};
    LineControlOutput output;
    uint8_t index;

    params.lineKpQ16 = Q16_ONE;
    params.derivativeAlphaQ16 = 0;
    params.baseSpeed = 20;
    params.maxTargetSpeed = 25;
    params.maxDeltaSpeed = 10;
    params.derivativeLimit = 1000;
    output = LineControl_Step(&state, &sample, &params, 0);
    if ((output.deltaSpeed != 6) || (output.leftTarget != 25) ||
        (output.rightTarget != 14) || !output.targetSaturated) {
        return 20;
    }

    params.lineKpQ16 = 0;
    params.lineKdQ16 = Q16_ONE;
    params.derivativeAlphaQ16 = Q16_ONE / 2;
    params.baseSpeed = 0;
    params.maxTargetSpeed = 100;
    params.maxDeltaSpeed = 100;
    params.derivativeLimit = 40;
    sample.error = 200;
    output = LineControl_Step(&state, &sample, &params, 0);
    if ((state.filteredDerivative != 20) || (output.deltaSpeed != 12) ||
        (output.leftTarget != 12) || (output.rightTarget != -12) ||
        output.targetSaturated) {
        return 21;
    }

    LineControl_Reset(&state);
    params.lineKpQ16 = Q16_ONE;
    params.lineKdQ16 = 0;
    params.derivativeAlphaQ16 = 0;
    params.baseSpeed = 0;
    params.maxTargetSpeed = 100;
    params.maxDeltaSpeed = 100;
    sample.error = 100;
    for (index = 0U; index < 9U; index++) {
        output = LineControl_Step(&state, &sample, &params, 0);
    }
    output = LineControl_Step(&state, &sample, &params, 70);
    if ((output.deltaSpeed != 48) || (output.leftTarget != 48) ||
        (output.rightTarget != -48)) {
        return 22;
    }
    return 0;
}

static int Test_KeyStart(void)
{
    KeyStartState state;

    KeyStart_Init(&state);
    if (KeyStart_OnSample(&state, false) ||
        !KeyStart_OnSample(&state, true) ||
        KeyStart_OnSample(&state, true) ||
        KeyStart_OnSample(&state, false) ||
        !KeyStart_OnSample(&state, true)) {
        return 30;
    }
    return 0;
}

int main(void)
{
    int result = Test_ControlParams();

    if (result != 0) {
        return result;
    }
    result = Test_SpeedPI();
    if (result != 0) {
        return result;
    }
    result = Test_LineControl();
    if (result != 0) {
        return result;
    }
    return Test_KeyStart();
}
