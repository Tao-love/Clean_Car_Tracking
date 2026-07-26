// ----- AI
/* 150 ms 丢线、300 ms 堵转与控制超期联锁。 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "autotune_types.h"
#include "motor.h"
#include "safety_guard.h"

static SafetyStatus gSafetyStatus;
static SafetyControlResetHandler gResetHandler;

static uint16_t SafetyGuard_IncrementSaturating(uint16_t value);

void SafetyGuard_Init(SafetyControlResetHandler resetHandler, uint32_t tick)
{
    gResetHandler = resetHandler;
    gSafetyStatus.lineLostTicks = 0U;
    gSafetyStatus.leftStallTicks = 0U;
    gSafetyStatus.rightStallTicks = 0U;
    gSafetyStatus.consecutiveOverruns = 0U;
    SafetyGuard_Stop(STOP_REASON_BOOT, FAULT_NONE, tick);
}

void SafetyGuard_BeginTrial(uint32_t tick)
{
    gSafetyStatus.lineLostTicks = 0U;
    gSafetyStatus.leftStallTicks = 0U;
    gSafetyStatus.rightStallTicks = 0U;
    gSafetyStatus.consecutiveOverruns = 0U;
    gSafetyStatus.stopReason = STOP_REASON_NONE;
    gSafetyStatus.fault = FAULT_NONE;
    gSafetyStatus.stopTick = tick;
}

bool SafetyGuard_Evaluate(uint32_t tick, const ControlSample *sample,
    const ControlParams *params, bool requireLine)
{
    /* ===== 参数完整性保护：控制样本或活动参数无效时立即停车 ===== */
    if ((sample == 0) || (params == 0)) {
        SafetyGuard_Stop(STOP_REASON_PARAMETER_ERROR, FAULT_INTERNAL, tick);
        return true;
    }

    /* ===== 循线丢失保护：仅循线模式检查灰度是否持续有效 ===== */
    if (requireLine) {
        gSafetyStatus.lineLostTicks = sample->line.valid ? 0U :
            SafetyGuard_IncrementSaturating(gSafetyStatus.lineLostTicks);
        if (gSafetyStatus.lineLostTicks >= SAFETY_LINE_LOST_TICKS) {
            SafetyGuard_Stop(STOP_REASON_LINE_LOST, FAULT_LINE_LOST, tick);
            return true;
        }
    } else {
        gSafetyStatus.lineLostTicks = 0U;
    }

    /* ===== 左右独立堵转保护：高 PWM 且低编码器速度持续 300 ms 才停车 ===== */
    gSafetyStatus.leftStallTicks =
        ((abs(sample->leftPwm) > params->stallPwmThreshold) &&
         (abs(sample->leftSpeed) < params->stallSpeedThreshold)) ?
        SafetyGuard_IncrementSaturating(gSafetyStatus.leftStallTicks) : 0U;
    gSafetyStatus.rightStallTicks =
        ((abs(sample->rightPwm) > params->stallPwmThreshold) &&
         (abs(sample->rightSpeed) < params->stallSpeedThreshold)) ?
        SafetyGuard_IncrementSaturating(gSafetyStatus.rightStallTicks) : 0U;
    if (gSafetyStatus.leftStallTicks >= SAFETY_STALL_TICKS) {
        SafetyGuard_Stop(STOP_REASON_STALL_LEFT, FAULT_STALL_LEFT, tick);
        return true;
    }
    if (gSafetyStatus.rightStallTicks >= SAFETY_STALL_TICKS) {
        SafetyGuard_Stop(STOP_REASON_STALL_RIGHT, FAULT_STALL_RIGHT, tick);
        return true;
    }
    return false;
}

bool SafetyGuard_ReportOverrun(
    uint32_t tick, uint16_t missedTicks, const ControlParams *params)
{
    /* ===== 控制超期保护：连续漏执行达到参数上限时停车 ===== */
    if ((params == 0) || (missedTicks == 0U)) {
        return false;
    }
    if ((UINT16_MAX - gSafetyStatus.consecutiveOverruns) < missedTicks) {
        gSafetyStatus.consecutiveOverruns = UINT16_MAX;
    } else {
        gSafetyStatus.consecutiveOverruns =
            (uint16_t) (gSafetyStatus.consecutiveOverruns + missedTicks);
    }
    if (gSafetyStatus.consecutiveOverruns >= params->controlOverrunLimit) {
        SafetyGuard_Stop(
            STOP_REASON_CONTROL_OVERRUN, FAULT_CONTROL_OVERRUN, tick);
        return true;
    }
    return false;
}

void SafetyGuard_ReportOnTimeTick(void)
{
    gSafetyStatus.consecutiveOverruns = 0U;
}

void SafetyGuard_Stop(StopReason reason, FaultCode fault, uint32_t tick)
{
    /* ===== 统一停车动作：所有安全路径均从这里清 PWM、方向和控制状态 ===== */
    Motor_Stop();
    if (gResetHandler != 0) {
        gResetHandler();
    }
    gSafetyStatus.stopReason = reason;
    gSafetyStatus.fault = fault;
    gSafetyStatus.stopTick = tick;
}

void SafetyGuard_ClearFault(uint32_t tick)
{
    Motor_Stop();
    if (gResetHandler != 0) {
        gResetHandler();
    }
    gSafetyStatus.stopReason = STOP_REASON_NONE;
    gSafetyStatus.fault = FAULT_NONE;
    gSafetyStatus.stopTick = tick;
    gSafetyStatus.lineLostTicks = 0U;
    gSafetyStatus.leftStallTicks = 0U;
    gSafetyStatus.rightStallTicks = 0U;
    gSafetyStatus.consecutiveOverruns = 0U;
}

SafetyStatus SafetyGuard_GetStatus(void)
{
    return gSafetyStatus;
}

static uint16_t SafetyGuard_IncrementSaturating(uint16_t value)
{
    return (value == UINT16_MAX) ? UINT16_MAX : (uint16_t) (value + 1U);
}
// ----- AI
