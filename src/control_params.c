// ----- AI
/* 参数校验、小端序列化和原子切换。 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "autotune_types.h"
#include "control_params.h"
#include "manual_tuning.h"

#define PARAM_GAIN_MAX_Q16          (64L * Q16_ONE)
#define PARAM_LINE_GAIN_MAX_Q16     (8L * Q16_ONE)
#define PARAM_INTEGRAL_LIMIT_MAX    (1000000L)
#define PARAM_SPEED_MAX             (1000)
#define PARAM_COMMISSIONING_PWM_MAX (400)
#define PARAM_DERIVATIVE_MAX        (7000)
#define PARAM_TELEMETRY_HZ_MAX      (20U)
#define PARAM_OVERRUN_LIMIT_MAX     (20U)

static ControlParams gActiveParams;
static ControlParams gPendingParams;
static bool gHasPending;
static uint16_t gParamVersion;

static int32_t ControlParams_ReadI32(const uint8_t *source);
static int16_t ControlParams_ReadI16(const uint8_t *source);
static uint16_t ControlParams_ReadU16(const uint8_t *source);
static void ControlParams_WriteI32(uint8_t *destination, int32_t value);
static void ControlParams_WriteI16(uint8_t *destination, int16_t value);
static void ControlParams_WriteU16(uint8_t *destination, uint16_t value);

void ControlParams_Init(
    const ControlParams *flashParams, uint16_t flashParamVersion)
{
    const ControlParams *manualParams = ManualTuning_GetParams();

    /* 手动调参模式只接受本次固件内的表；历史 Flash 记录不得覆盖它。 */
    (void) flashParams;
    (void) flashParamVersion;
    if (manualParams != 0) {
        gActiveParams = *manualParams;
    } else {
        (void) memset(&gActiveParams, 0, sizeof(gActiveParams));
    }
    (void) memset(&gPendingParams, 0, sizeof(gPendingParams));
    gHasPending = false;
    gParamVersion = 0U;
}

ParamsValidationResult ControlParams_Validate(const ControlParams *params)
{
    if (params == 0) {
        return PARAMS_NULL;
    }
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
        return PARAMS_GAIN_RANGE;
    }
    if ((params->speedIntegralLimit < 0) ||
        (params->speedIntegralLimit > PARAM_INTEGRAL_LIMIT_MAX) ||
        (params->baseSpeed < 0) ||
        (params->baseSpeed > PARAM_SPEED_MAX) ||
        (params->maxTargetSpeed <= 0) ||
        (params->maxTargetSpeed > PARAM_SPEED_MAX) ||
        (params->maxDeltaSpeed < 0) ||
        (params->maxDeltaSpeed > PARAM_SPEED_MAX) ||
        (params->maxPwm < 0) ||
        (params->maxPwm > PARAM_COMMISSIONING_PWM_MAX) ||
        (params->derivativeLimit < 0) ||
        (params->derivativeLimit > PARAM_DERIVATIVE_MAX) ||
        (params->stallPwmThreshold < 0) ||
        (params->stallPwmThreshold > PARAM_COMMISSIONING_PWM_MAX) ||
        (params->stallSpeedThreshold <= 0) ||
        (params->stallSpeedThreshold > PARAM_SPEED_MAX) ||
        (params->telemetryHz > PARAM_TELEMETRY_HZ_MAX) ||
        (params->controlOverrunLimit == 0U) ||
        (params->controlOverrunLimit > PARAM_OVERRUN_LIMIT_MAX)) {
        return PARAMS_LIMIT_RANGE;
    }
    if ((params->baseSpeed > params->maxTargetSpeed) ||
        (params->stallPwmThreshold >= params->maxPwm)) {
        return PARAMS_RELATION_INVALID;
    }
    return PARAMS_VALID;
}

ParamsValidationResult ControlParams_Stage(
    const ControlParams *params, SystemState state)
{
    ParamsValidationResult result;

    if (state == SYSTEM_STATE_RUNNING) {
        return PARAMS_RUNNING_LOCKED;
    }
    if (gHasPending) {
        return PARAMS_ALREADY_PENDING;
    }
    result = ControlParams_Validate(params);
    if (result != PARAMS_VALID) {
        return result;
    }
    gPendingParams = *params;
    gHasPending = true;
    return PARAMS_VALID;
}

bool ControlParams_ApplyPendingAtBoundary(SystemState state)
{
    if (!gHasPending || (state == SYSTEM_STATE_RUNNING)) {
        return false;
    }
    gActiveParams = gPendingParams;
    gHasPending = false;
    gParamVersion++;
    if (gParamVersion == 0U) {
        gParamVersion = 1U;
    }
    return true;
}

const ControlParams *ControlParams_GetActive(void)
{
    return &gActiveParams;
}

uint16_t ControlParams_GetVersion(void)
{
    return gParamVersion;
}

bool ControlParams_HasPending(void)
{
    return gHasPending;
}

bool ControlParams_DecodeWire(
    const uint8_t *payload, uint16_t length, ControlParams *params)
{
    const uint8_t *cursor = payload;

    if ((payload == 0) || (params == 0) ||
        (length != CONTROL_PARAMS_WIRE_SIZE)) {
        return false;
    }
#define READ_I32_FIELD(field) do { params->field = ControlParams_ReadI32(cursor); cursor += 4; } while (0)
#define READ_I16_FIELD(field) do { params->field = ControlParams_ReadI16(cursor); cursor += 2; } while (0)
#define READ_U16_FIELD(field) do { params->field = ControlParams_ReadU16(cursor); cursor += 2; } while (0)
    READ_I32_FIELD(speedKpLeftQ16);
    READ_I32_FIELD(speedKiLeftQ16);
    READ_I32_FIELD(speedFeedforwardLeftQ16);
    READ_I32_FIELD(speedKpRightQ16);
    READ_I32_FIELD(speedKiRightQ16);
    READ_I32_FIELD(speedFeedforwardRightQ16);
    READ_I32_FIELD(lineKpQ16);
    READ_I32_FIELD(lineKdQ16);
    READ_I32_FIELD(derivativeAlphaQ16);
    READ_I32_FIELD(speedIntegralLimit);
    READ_I16_FIELD(baseSpeed);
    READ_I16_FIELD(maxTargetSpeed);
    READ_I16_FIELD(maxDeltaSpeed);
    READ_I16_FIELD(maxPwm);
    READ_I16_FIELD(derivativeLimit);
    READ_I16_FIELD(stallPwmThreshold);
    READ_I16_FIELD(stallSpeedThreshold);
    READ_U16_FIELD(telemetryHz);
    READ_U16_FIELD(controlOverrunLimit);
#undef READ_I32_FIELD
#undef READ_I16_FIELD
#undef READ_U16_FIELD
    return true;
}

bool ControlParams_EncodeWire(
    const ControlParams *params, uint8_t *payload, uint16_t capacity)
{
    uint8_t *cursor = payload;

    if ((params == 0) || (payload == 0) ||
        (capacity < CONTROL_PARAMS_WIRE_SIZE)) {
        return false;
    }
#define WRITE_I32_FIELD(field) do { ControlParams_WriteI32(cursor, params->field); cursor += 4; } while (0)
#define WRITE_I16_FIELD(field) do { ControlParams_WriteI16(cursor, params->field); cursor += 2; } while (0)
#define WRITE_U16_FIELD(field) do { ControlParams_WriteU16(cursor, params->field); cursor += 2; } while (0)
    WRITE_I32_FIELD(speedKpLeftQ16);
    WRITE_I32_FIELD(speedKiLeftQ16);
    WRITE_I32_FIELD(speedFeedforwardLeftQ16);
    WRITE_I32_FIELD(speedKpRightQ16);
    WRITE_I32_FIELD(speedKiRightQ16);
    WRITE_I32_FIELD(speedFeedforwardRightQ16);
    WRITE_I32_FIELD(lineKpQ16);
    WRITE_I32_FIELD(lineKdQ16);
    WRITE_I32_FIELD(derivativeAlphaQ16);
    WRITE_I32_FIELD(speedIntegralLimit);
    WRITE_I16_FIELD(baseSpeed);
    WRITE_I16_FIELD(maxTargetSpeed);
    WRITE_I16_FIELD(maxDeltaSpeed);
    WRITE_I16_FIELD(maxPwm);
    WRITE_I16_FIELD(derivativeLimit);
    WRITE_I16_FIELD(stallPwmThreshold);
    WRITE_I16_FIELD(stallSpeedThreshold);
    WRITE_U16_FIELD(telemetryHz);
    WRITE_U16_FIELD(controlOverrunLimit);
#undef WRITE_I32_FIELD
#undef WRITE_I16_FIELD
#undef WRITE_U16_FIELD
    return true;
}

static int32_t ControlParams_ReadI32(const uint8_t *source)
{
    uint32_t value = (uint32_t) source[0] |
        ((uint32_t) source[1] << 8) | ((uint32_t) source[2] << 16) |
        ((uint32_t) source[3] << 24);
    return (int32_t) value;
}

static int16_t ControlParams_ReadI16(const uint8_t *source)
{
    return (int16_t) ControlParams_ReadU16(source);
}

static uint16_t ControlParams_ReadU16(const uint8_t *source)
{
    return (uint16_t) source[0] | ((uint16_t) source[1] << 8);
}

static void ControlParams_WriteI32(uint8_t *destination, int32_t value)
{
    uint32_t raw = (uint32_t) value;
    destination[0] = (uint8_t) raw;
    destination[1] = (uint8_t) (raw >> 8);
    destination[2] = (uint8_t) (raw >> 16);
    destination[3] = (uint8_t) (raw >> 24);
}

static void ControlParams_WriteI16(uint8_t *destination, int16_t value)
{
    ControlParams_WriteU16(destination, (uint16_t) value);
}

static void ControlParams_WriteU16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t) value;
    destination[1] = (uint8_t) (value >> 8);
}
// ----- AI
