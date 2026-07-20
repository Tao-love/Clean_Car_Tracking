// ----- AI
/* 循线 PD、数字差分低通和目标速度限幅。 */

#include <stdbool.h>
#include <stdint.h>

#include "autotune_types.h"
#include "line_control.h"

static int32_t LineControl_ClampI32(int32_t value, int32_t limit);
static int16_t LineControl_ClampTarget(
    int32_t value, int16_t limit, bool *saturated);

void LineControl_Reset(LineControlState *state)
{
    if (state != 0) {
        state->previousError = 0;
        state->filteredDerivative = 0;
        state->initialized = false;
    }
}

LineControlOutput LineControl_Step(LineControlState *state,
    const LineSensorSample *sample, const ControlParams *params)
{
    LineControlOutput result = {0, 0, 0, false};
    int32_t rawDerivative;
    int32_t filtered;
    int64_t deltaQ16;
    int32_t delta;

    if ((state == 0) || (sample == 0) || (params == 0) || !sample->valid) {
        return result;
    }

    rawDerivative = state->initialized ?
        ((int32_t) sample->error - state->previousError) : 0;
    rawDerivative = LineControl_ClampI32(
        rawDerivative, params->derivativeLimit);
    filtered = (int32_t) ((((int64_t) params->derivativeAlphaQ16 *
        state->filteredDerivative) +
        ((int64_t) (Q16_ONE - params->derivativeAlphaQ16) * rawDerivative)) /
        Q16_ONE);
    state->filteredDerivative = filtered;
    state->previousError = sample->error;
    state->initialized = true;

    deltaQ16 = ((int64_t) params->lineKpQ16 * sample->error) +
        ((int64_t) params->lineKdQ16 * filtered);
    delta = (int32_t) (deltaQ16 / Q16_ONE);
    delta = LineControl_ClampI32(delta, params->maxDeltaSpeed);
    result.deltaSpeed = (int16_t) delta;
    result.leftTarget = LineControl_ClampTarget(
        (int32_t) params->baseSpeed - delta,
        params->maxTargetSpeed, &result.targetSaturated);
    result.rightTarget = LineControl_ClampTarget(
        (int32_t) params->baseSpeed + delta,
        params->maxTargetSpeed, &result.targetSaturated);
    return result;
}

static int32_t LineControl_ClampI32(int32_t value, int32_t limit)
{
    if (limit < 0) {
        limit = -limit;
    }
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

static int16_t LineControl_ClampTarget(
    int32_t value, int16_t limit, bool *saturated)
{
    if (value > limit) {
        *saturated = true;
        return limit;
    }
    if (value < -limit) {
        *saturated = true;
        return (int16_t) -limit;
    }
    return (int16_t) value;
}
// ----- AI
