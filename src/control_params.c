/* 实车验证过的 150 Hz 固定控制参数。 */

#include <stdbool.h>
#include <stdint.h>

#include "control_params.h"
#include "control_types.h"

#define PARAM_GAIN_MAX_Q16          (1000L * Q16_ONE)
#define PARAM_LINE_GAIN_MAX_Q16     (8L * Q16_ONE)
#define PARAM_INTEGRAL_LIMIT_MAX    (1000000L)
#define PARAM_SPEED_MAX             (1000)
#define PARAM_PWM_MAX               (1000)
#define PARAM_DERIVATIVE_MAX        (7000)
#define PARAM_DISTANCE_COUNTS_MAX   (1000000L)

/* KEY2/KEY3 先复制 KEY1 的 15 个控制参数；距离值是待编码器实测的临时占位。 */
static ControlModeProfile gProfiles[CONTROL_PROFILE_COUNT] = {
    {
        {
            12L * Q16_ONE, 0, 42L * Q16_ONE,
            12L * Q16_ONE, 0, (69L * Q16_ONE / 2),
            (Q16_ONE / 70), (Q16_ONE / 320),
            (5L * Q16_ONE / 8), 10000,
            15, 67, 35,
            600, 2333
        },
        0
    },
    {
        {
            12L * Q16_ONE, 0, 42L * Q16_ONE,
            12L * Q16_ONE, 0, (69L * Q16_ONE / 2),
            (Q16_ONE / 70), (Q16_ONE / 320),
            (5L * Q16_ONE / 8), 10000,
            15, 67, 35,
            600, 2333
        },
        1200
    },
    {
        {
            12L * Q16_ONE, 0, 42L * Q16_ONE,
            12L * Q16_ONE, 0, (69L * Q16_ONE / 2),
            (Q16_ONE / 70), (Q16_ONE / 320),
            (5L * Q16_ONE / 8), 10000,
            15, 67, 35,
            600, 2333
        },
        8000
    }
};

static bool ControlParams_IsValid(const ControlParams *params)
{
    if ((params->speedKpLeftQ16 < 0) ||
        (params->speedKpLeftQ16 > PARAM_GAIN_MAX_Q16) ||
        (params->speedKiLeftQ16 < 0) ||
        (params->speedKiLeftQ16 > PARAM_GAIN_MAX_Q16) ||
        (params->speedFeedforwardLeftQ16 < 0) ||
        (params->speedFeedforwardLeftQ16 > PARAM_GAIN_MAX_Q16) ||
        (params->speedKpRightQ16 < 0) ||
        (params->speedKpRightQ16 > PARAM_GAIN_MAX_Q16) ||
        (params->speedKiRightQ16 < 0) ||
        (params->speedKiRightQ16 > PARAM_GAIN_MAX_Q16) ||
        (params->speedFeedforwardRightQ16 < 0) ||
        (params->speedFeedforwardRightQ16 > PARAM_GAIN_MAX_Q16) ||
        (params->lineKpQ16 < 0) ||
        (params->lineKpQ16 > PARAM_LINE_GAIN_MAX_Q16) ||
        (params->lineKdQ16 < 0) ||
        (params->lineKdQ16 > PARAM_LINE_GAIN_MAX_Q16) ||
        (params->derivativeAlphaQ16 < 0) ||
        (params->derivativeAlphaQ16 > Q16_ONE)) {
        return false;
    }
    if ((params->speedIntegralLimit < 0) ||
        (params->speedIntegralLimit > PARAM_INTEGRAL_LIMIT_MAX) ||
        (params->baseSpeed < 0) ||
        (params->baseSpeed > PARAM_SPEED_MAX) ||
        (params->maxTargetSpeed <= 0) ||
        (params->maxTargetSpeed > PARAM_SPEED_MAX) ||
        (params->maxDeltaSpeed < 0) ||
        (params->maxDeltaSpeed > PARAM_SPEED_MAX) ||
        (params->maxPwm <= 0) || (params->maxPwm > PARAM_PWM_MAX) ||
        (params->derivativeLimit < 0) ||
        (params->derivativeLimit > PARAM_DERIVATIVE_MAX)) {
        return false;
    }
    return params->baseSpeed <= params->maxTargetSpeed;
}

static bool ControlProfile_IsValid(ControlProfileId id,
    const ControlModeProfile *profile)
{
    if ((profile == 0) || !ControlParams_IsValid(&profile->params)) {
        return false;
    }
    if (id == CONTROL_PROFILE_KEY1) {
        return profile->distanceCounts == 0;
    }
    return (profile->distanceCounts > 0) &&
        (profile->distanceCounts <= PARAM_DISTANCE_COUNTS_MAX);
}

const ControlParams *ControlParams_Get(void)
{
    return ControlProfile_IsValid(CONTROL_PROFILE_KEY1,
        &gProfiles[CONTROL_PROFILE_KEY1]) ?
        &gProfiles[CONTROL_PROFILE_KEY1].params : 0;
}

const ControlModeProfile *ControlParams_GetProfile(ControlProfileId id)
{
    if ((id < CONTROL_PROFILE_KEY1) || (id >= CONTROL_PROFILE_COUNT) ||
        !ControlProfile_IsValid(id, &gProfiles[id])) {
        return 0;
    }
    return &gProfiles[id];
}

ControlParams *ControlParams_GetMutableForUart(void)
{
    return &gProfiles[CONTROL_PROFILE_KEY1].params;
}
