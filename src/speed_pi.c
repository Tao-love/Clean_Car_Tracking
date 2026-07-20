// ----- AI
/* 速度 PI 实现；所有乘法使用 64 位中间量防止 Q16.16 溢出。 */

#include <stdbool.h>
#include <stdint.h>

#include "autotune_types.h"
#include "speed_pi.h"

static int32_t SpeedPI_ClampI32(int32_t value, int32_t limit);
static int16_t SpeedPI_ClampI16(int64_t value, int16_t limit, bool *saturated);
static int64_t SpeedPI_CalculateRaw(int16_t target, int32_t error,
    int32_t integral, int32_t kpQ16, int32_t kiQ16, int32_t feedforwardQ16);

void SpeedPI_Reset(SpeedPIState *state)
{
    if (state != 0) {
        state->integral = 0;
    }
}

SpeedPIOutput SpeedPI_Step(SpeedPIState *state, int16_t target,
    int16_t actual, int32_t kpQ16, int32_t kiQ16, int32_t feedforwardQ16,
    int32_t integralLimit, int16_t pwmLimit)
{
    SpeedPIOutput result = {0, false};
    int32_t error;
    int32_t candidateIntegral;
    int64_t candidateRaw;
    bool candidateSaturated = false;
    int16_t candidatePwm;

    if ((state == 0) || (target == 0) || (pwmLimit <= 0)) {
        SpeedPI_Reset(state);
        return result;
    }

    error = (int32_t) target - actual;
    candidateIntegral = SpeedPI_ClampI32(
        state->integral + error, integralLimit);
    candidateRaw = SpeedPI_CalculateRaw(target, error, candidateIntegral,
        kpQ16, kiQ16, feedforwardQ16);
    candidatePwm = SpeedPI_ClampI16(
        candidateRaw, pwmLimit, &candidateSaturated);

    if (!candidateSaturated ||
        ((candidatePwm >= pwmLimit) && (error < 0)) ||
        ((candidatePwm <= -pwmLimit) && (error > 0))) {
        state->integral = candidateIntegral;
    } else {
        candidateRaw = SpeedPI_CalculateRaw(target, error, state->integral,
            kpQ16, kiQ16, feedforwardQ16);
        candidatePwm = SpeedPI_ClampI16(
            candidateRaw, pwmLimit, &candidateSaturated);
    }

    result.pwm = candidatePwm;
    result.saturated = candidateSaturated;
    return result;
}

static int32_t SpeedPI_ClampI32(int32_t value, int32_t limit)
{
    if (limit <= 0) {
        return 0;
    }
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

static int16_t SpeedPI_ClampI16(int64_t value, int16_t limit, bool *saturated)
{
    if (value > limit) {
        *saturated = true;
        return limit;
    }
    if (value < -limit) {
        *saturated = true;
        return (int16_t) -limit;
    }
    *saturated = false;
    return (int16_t) value;
}

static int64_t SpeedPI_CalculateRaw(int16_t target, int32_t error,
    int32_t integral, int32_t kpQ16, int32_t kiQ16, int32_t feedforwardQ16)
{
    int64_t q16Sum = ((int64_t) feedforwardQ16 * target) +
        ((int64_t) kpQ16 * error) + ((int64_t) kiQ16 * integral);
    return q16Sum / Q16_ONE;
}
// ----- AI
