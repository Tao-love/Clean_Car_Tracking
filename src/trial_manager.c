// ----- AI
/* 本地 100 Hz 双闭环、500 tick 试验、状态转移与汇总生成。 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "adc.h"
#include "autotune_types.h"
#include "control_params.h"
#include "encoder.h"
#include "line_control.h"
#include "motor.h"
#include "safety_guard.h"
#include "speed_pi.h"
#include "trial_manager.h"
#include "trial_stats.h"

static TrialManagerStatus gStatus;
static TrialSummary gFinishedSummary;
static LineControlState gLineState;
static SpeedPIState gLeftPiState;
static SpeedPIState gRightPiState;
static bool gParamsApplied;
static bool gOverrunReportedForBoundary;

static void TrialManager_ResetControlState(void);
static void TrialManager_Finish(
    uint32_t tick, StopReason reason, FaultCode fault, SystemState state);

void TrialManager_Init(uint32_t tick, const ControlParams *flashParams)
{
    (void) memset(&gStatus, 0, sizeof(gStatus));
    (void) memset(&gFinishedSummary, 0, sizeof(gFinishedSummary));
    gStatus.state = SYSTEM_STATE_BOOT_SAFE;
    gStatus.controlTick = tick;
    Encoder_Init();
    ControlParams_Init(flashParams);
    TrialManager_ResetControlState();
    SafetyGuard_Init(TrialManager_ResetControlState, tick);
    gStatus.safety = SafetyGuard_GetStatus();
    gStatus.state = SYSTEM_STATE_IDLE;
    gParamsApplied = false;
    gOverrunReportedForBoundary = false;
}

void TrialManager_ControlTick(uint32_t tick)
{
    const ControlParams *params;
    LineSensorSample line;

    gStatus.controlTick = tick;
    if (!gOverrunReportedForBoundary) {
        SafetyGuard_ReportOnTimeTick();
    }
    gOverrunReportedForBoundary = false;
    gParamsApplied = ControlParams_ApplyPendingAtBoundary(gStatus.state);
    gStatus.paramVersion = ControlParams_GetVersion();
    Encoder_UpdateSpeeds();
    line = LineSensor_ReadSample();
    gStatus.latestSample.tick = tick;
    gStatus.latestSample.line = line;
    gStatus.latestSample.leftSpeed = Encoder_GetLeftSpeed();
    gStatus.latestSample.rightSpeed = Encoder_GetRightSpeed();

    if (gStatus.state != SYSTEM_STATE_RUNNING) {
        gStatus.safety = SafetyGuard_GetStatus();
        return;
    }

    params = ControlParams_GetActive();
    {
        LineControlOutput lineOutput =
            LineControl_Step(&gLineState, &line, params);
        SpeedPIOutput leftOutput = SpeedPI_Step(&gLeftPiState,
            lineOutput.leftTarget, gStatus.latestSample.leftSpeed,
            params->speedKpLeftQ16, params->speedKiLeftQ16,
            params->speedFeedforwardLeftQ16, params->speedIntegralLimit,
            params->maxPwm);
        SpeedPIOutput rightOutput = SpeedPI_Step(&gRightPiState,
            lineOutput.rightTarget, gStatus.latestSample.rightSpeed,
            params->speedKpRightQ16, params->speedKiRightQ16,
            params->speedFeedforwardRightQ16, params->speedIntegralLimit,
            params->maxPwm);

        gStatus.latestSample.leftTarget = lineOutput.leftTarget;
        gStatus.latestSample.rightTarget = lineOutput.rightTarget;
        gStatus.latestSample.leftPwm = leftOutput.pwm;
        gStatus.latestSample.rightPwm = rightOutput.pwm;
        gStatus.latestSample.targetSaturated = lineOutput.targetSaturated;
        gStatus.latestSample.pwmSaturated =
            leftOutput.saturated || rightOutput.saturated;
        Motor_SetSpeed(leftOutput.pwm, rightOutput.pwm);
    }

    TrialStats_AddSample(&gStatus.latestSample);
    gStatus.trialTicks++;
    if (SafetyGuard_Evaluate(tick, &gStatus.latestSample, params)) {
        SafetyStatus safety = SafetyGuard_GetStatus();
        TrialManager_Finish(
            tick, safety.stopReason, safety.fault, SYSTEM_STATE_FAULT);
        return;
    }
    if (gStatus.trialTicks >= TRIAL_DURATION_TICKS) {
        TrialManager_Finish(tick, STOP_REASON_TRIAL_COMPLETE,
            FAULT_NONE, SYSTEM_STATE_FINISHED);
    }
}

void TrialManager_OnValidFrame(uint32_t tick)
{
    SafetyGuard_OnValidFrame(tick);
}

void TrialManager_ReportOverrun(uint32_t tick, uint16_t missedTicks)
{
    if (gStatus.state != SYSTEM_STATE_RUNNING) {
        return;
    }
    gStatus.controlOverruns += missedTicks;
    gOverrunReportedForBoundary = true;
    TrialStats_AddOverruns(missedTicks);
    if (SafetyGuard_ReportOverrun(
            tick, missedTicks, ControlParams_GetActive())) {
        SafetyStatus safety = SafetyGuard_GetStatus();
        TrialManager_Finish(
            tick, safety.stopReason, safety.fault, SYSTEM_STATE_FAULT);
    }
}

ParamsValidationResult TrialManager_StageParams(const ControlParams *params)
{
    return ControlParams_Stage(params, gStatus.state);
}

TrialCommandResult TrialManager_Arm(uint16_t paramVersion)
{
    if (gStatus.state != SYSTEM_STATE_IDLE) {
        return TRIAL_COMMAND_BAD_STATE;
    }
    if (ControlParams_HasPending() ||
        (paramVersion != ControlParams_GetVersion())) {
        return TRIAL_COMMAND_BAD_VERSION;
    }
    if (ControlParams_Validate(ControlParams_GetActive()) != PARAMS_VALID) {
        return TRIAL_COMMAND_BAD_PARAMS;
    }
    Motor_Stop();
    TrialManager_ResetControlState();
    gStatus.state = SYSTEM_STATE_ARMED;
    return TRIAL_COMMAND_OK;
}

TrialCommandResult TrialManager_Start(uint32_t tick)
{
    if (gStatus.state != SYSTEM_STATE_ARMED) {
        return TRIAL_COMMAND_BAD_STATE;
    }
    SafetyGuard_Stop(STOP_REASON_NONE, FAULT_NONE, tick);
    TrialStats_Reset(ControlParams_GetVersion(),
        Encoder_GetLeftCount(), Encoder_GetRightCount());
    SafetyGuard_BeginTrial(tick);
    (void) memset(&gStatus.latestSample, 0, sizeof(gStatus.latestSample));
    gStatus.trialTicks = 0U;
    gStatus.summaryReady = false;
    gStatus.state = SYSTEM_STATE_RUNNING;
    return TRIAL_COMMAND_OK;
}

TrialCommandResult TrialManager_Abort(uint32_t tick)
{
    if (gStatus.state != SYSTEM_STATE_RUNNING) {
        return TRIAL_COMMAND_BAD_STATE;
    }
    TrialManager_Finish(
        tick, STOP_REASON_ABORTED, FAULT_NONE, SYSTEM_STATE_FINISHED);
    return TRIAL_COMMAND_OK;
}

TrialCommandResult TrialManager_ClearFault(uint32_t tick)
{
    if (gStatus.state != SYSTEM_STATE_FAULT) {
        return TRIAL_COMMAND_BAD_STATE;
    }
    if (!LineSensor_ReadSample().valid ||
        (abs(Encoder_GetLeftSpeed()) > 1) ||
        (abs(Encoder_GetRightSpeed()) > 1)) {
        return TRIAL_COMMAND_CONDITION_UNSAFE;
    }
    SafetyGuard_ClearFault(tick);
    gStatus.safety = SafetyGuard_GetStatus();
    gStatus.summaryReady = false;
    gStatus.state = SYSTEM_STATE_IDLE;
    return TRIAL_COMMAND_OK;
}

TrialManagerStatus TrialManager_GetStatus(void)
{
    gStatus.paramVersion = ControlParams_GetVersion();
    gStatus.safety = SafetyGuard_GetStatus();
    return gStatus;
}

bool TrialManager_GetSummary(TrialSummary *summary)
{
    if ((summary == 0) || !gStatus.summaryReady) {
        return false;
    }
    *summary = gFinishedSummary;
    return true;
}

void TrialManager_MarkSummaryQueued(void)
{
    gStatus.summaryReady = false;
    if (gStatus.state == SYSTEM_STATE_FINISHED) {
        gStatus.state = SYSTEM_STATE_IDLE;
    }
}

bool TrialManager_ConsumeParamsApplied(void)
{
    bool result = gParamsApplied;
    gParamsApplied = false;
    return result;
}

static void TrialManager_ResetControlState(void)
{
    LineControl_Reset(&gLineState);
    SpeedPI_Reset(&gLeftPiState);
    SpeedPI_Reset(&gRightPiState);
    gStatus.latestSample.leftTarget = 0;
    gStatus.latestSample.rightTarget = 0;
    gStatus.latestSample.leftPwm = 0;
    gStatus.latestSample.rightPwm = 0;
}

static void TrialManager_Finish(
    uint32_t tick, StopReason reason, FaultCode fault, SystemState state)
{
    SafetyGuard_Stop(reason, fault, tick);
    gFinishedSummary = TrialStats_Finish(reason, fault,
        Encoder_GetLeftCount(), Encoder_GetRightCount());
    gStatus.safety = SafetyGuard_GetStatus();
    gStatus.summaryReady = true;
    gStatus.state = state;
}
// ----- AI
