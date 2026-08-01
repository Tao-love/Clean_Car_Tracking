/* 150 Hz 循线、速度 PI 与 10 秒运行上限。 */

#include <stdbool.h>
#include <stdint.h>

#include "UART_Auto_PID.h"
#include "control_params.h"
#include "control_types.h"
#include "encoder.h"
#include "gray_sensor.h"
#include "line_control.h"
#include "line_run.h"
#include "motor.h"
#include "mpu6050.h"
#include "Oled_Timer.h"
#include "speed_pi.h"

static const ControlParams *gParams;
static const ControlModeProfile *gProfile;
static LineControlState gLineState;
static SpeedPIState gLeftPiState;
static SpeedPIState gRightPiState;
static uint16_t gRunTicks;
static int32_t gStartLeftCount;
static int32_t gStartRightCount;
static int32_t gTargetDistanceCounts;
static int16_t gLastLineError;
static bool gHasLastLineError;
static bool gRunning;
static bool gBenchRunning;
static int16_t gBenchTarget;

typedef enum {
    LINE_RUN_MODE_STOPPED = 0,
    LINE_RUN_MODE_TIMED,
    LINE_RUN_MODE_STRAIGHT,
    LINE_RUN_MODE_LINE,
    LINE_RUN_MODE_BENCH
} LineRunMode;

static LineRunMode gMode;

#define LINE_RUN_BENCH_DEFAULT_TARGET (5)

static bool LineRun_StartProfile(ControlProfileId profileId,
    LineRunMode mode);
static bool LineRun_HasReachedDistance(void);
static int32_t LineRun_AbsI32(int32_t value);

static void LineRun_ResetControllerState(void)
{
    LineControl_Reset(&gLineState);
    SpeedPI_Reset(&gLeftPiState);
    SpeedPI_Reset(&gRightPiState);
    gLastLineError = 0;
    gHasLastLineError = false;
}

static void LineRun_ResetControlState(void)
{
    LineRun_ResetControllerState();
    gRunTicks = 0U;
    gStartLeftCount = 0;
    gStartRightCount = 0;
    gTargetDistanceCounts = 0;
}

bool LineRun_Init(void)
{
    gProfile = ControlParams_GetProfile(CONTROL_PROFILE_KEY1);
    gParams = (gProfile == 0) ? 0 : &gProfile->params;
    Encoder_Init();
    gRunning = false;
    gBenchRunning = false;
    gMode = LINE_RUN_MODE_STOPPED;
    gBenchTarget = LINE_RUN_BENCH_DEFAULT_TARGET;
    LineRun_ResetControlState();
    Motor_Stop();
    return gParams != 0;
}

bool LineRun_Start(void)
{
    return LineRun_StartProfile(CONTROL_PROFILE_KEY1,
        LINE_RUN_MODE_TIMED);
}

bool LineRun_StartStraight(void)
{
    return LineRun_StartProfile(CONTROL_PROFILE_KEY2,
        LINE_RUN_MODE_STRAIGHT);
}

bool LineRun_StartLine(void)
{
    return LineRun_StartProfile(CONTROL_PROFILE_KEY3,
        LINE_RUN_MODE_LINE);
}

bool LineRun_StartBench(void)
{
    return LineRun_StartProfile(CONTROL_PROFILE_KEY1,
        LINE_RUN_MODE_BENCH);
}

bool LineRun_SetBenchTarget(int16_t target)
{
    if (!gRunning || !gBenchRunning || (target < 1) || (target > 8)) {
        return false;
    }
    gBenchTarget = target;
    return true;
}

void LineRun_Stop(void)
{
    Motor_Stop();
    OledTimer_StopTiming();
    gRunning = false;
    gBenchRunning = false;
    gMode = LINE_RUN_MODE_STOPPED;
    gBenchTarget = LINE_RUN_BENCH_DEFAULT_TARGET;
    UART_Auto_PID_OnRunStopped();
    LineRun_ResetControlState();
}

void LineRun_ResetForTuningUpdate(void)
{
    LineRun_ResetControllerState();
}

void LineRun_ControlTick(void)
{
    LineSensorSample line;
    LineControlOutput lineOutput;
    SpeedPIOutput leftOutput;
    SpeedPIOutput rightOutput;
    UARTAutoPidControlSample controlSample;
    int16_t leftSpeed;
    int16_t rightSpeed;

    if (!gRunning) {
        return;
    }

    Encoder_UpdateSpeeds();
    if (LineRun_HasReachedDistance()) {
        LineRun_Stop();
        return;
    }
    if (gBenchRunning) {
        line.error = 0;
        lineOutput.leftTarget = gBenchTarget;
        lineOutput.rightTarget = gBenchTarget;
    } else if (gMode == LINE_RUN_MODE_STRAIGHT) {
        line.error = 0;
        lineOutput.leftTarget = gParams->baseSpeed;
        lineOutput.rightTarget = gParams->baseSpeed;
    } else {
        line = LineSensor_ReadSample();
        if (line.valid) {
            gLastLineError = line.error;
            gHasLastLineError = true;
        } else {
            line.valid = true;
            line.error = gHasLastLineError ? gLastLineError : 0;
        }
        MPU6050_Update();
        lineOutput = LineControl_Step(
            &gLineState, &line, gParams, MPU6050_GetTurnDamping());
    }
    leftSpeed = Encoder_GetLeftSpeed();
    rightSpeed = Encoder_GetRightSpeed();
    leftOutput = SpeedPI_Step(&gLeftPiState,
        lineOutput.leftTarget, leftSpeed,
        gParams->speedKpLeftQ16, gParams->speedKiLeftQ16,
        gParams->speedFeedforwardLeftQ16, gParams->speedIntegralLimit,
        gParams->maxPwm);
    rightOutput = SpeedPI_Step(&gRightPiState,
        lineOutput.rightTarget, rightSpeed,
        gParams->speedKpRightQ16, gParams->speedKiRightQ16,
        gParams->speedFeedforwardRightQ16, gParams->speedIntegralLimit,
        gParams->maxPwm);
    Motor_SetSpeed(leftOutput.pwm, rightOutput.pwm);
    controlSample.timestampMs = ((uint32_t) gRunTicks * 20U) / 3U;
    controlSample.lineError = line.error;
    controlSample.leftTarget = lineOutput.leftTarget;
    controlSample.leftSpeed = leftSpeed;
    controlSample.rightTarget = lineOutput.rightTarget;
    controlSample.rightSpeed = rightSpeed;
    controlSample.leftPwm = leftOutput.pwm;
    controlSample.rightPwm = rightOutput.pwm;
    controlSample.running = gRunning;
    UART_Auto_PID_OnControlSample(&controlSample);

    gRunTicks++;
    if (gRunTicks >= LINE_RUN_DURATION_TICKS) {
        LineRun_Stop();
    }
}

bool LineRun_IsRunning(void)
{
    return gRunning;
}

bool LineRun_IsBenchRunning(void)
{
    return gRunning && gBenchRunning;
}

static bool LineRun_StartProfile(ControlProfileId profileId,
    LineRunMode mode)
{
    const ControlModeProfile *profile;

    if (gRunning) {
        return false;
    }
    profile = ControlParams_GetProfile(profileId);
    if (profile == 0) {
        return false;
    }
    if (((mode == LINE_RUN_MODE_STRAIGHT) ||
        (mode == LINE_RUN_MODE_LINE)) &&
        (profile->distanceCounts <= 0)) {
        return false;
    }
    LineRun_ResetControlState();
    gProfile = profile;
    gParams = &profile->params;
    gStartLeftCount = Encoder_GetLeftCount();
    gStartRightCount = Encoder_GetRightCount();
    gTargetDistanceCounts = profile->distanceCounts;
    gBenchTarget = LINE_RUN_BENCH_DEFAULT_TARGET;
    gMode = mode;
    gBenchRunning = mode == LINE_RUN_MODE_BENCH;
    gRunning = true;
    OledTimer_StartTiming();
    return true;
}

static bool LineRun_HasReachedDistance(void)
{
    int32_t leftDelta;
    int32_t rightDelta;
    int32_t averageDelta;

    if ((gMode != LINE_RUN_MODE_STRAIGHT) &&
        (gMode != LINE_RUN_MODE_LINE)) {
        return false;
    }
    leftDelta = LineRun_AbsI32(
        Encoder_GetLeftCount() - gStartLeftCount);
    rightDelta = LineRun_AbsI32(
        Encoder_GetRightCount() - gStartRightCount);
    averageDelta = (leftDelta + rightDelta) / 2;
    return averageDelta >= gTargetDistanceCounts;
}

static int32_t LineRun_AbsI32(int32_t value)
{
    return (value < 0) ? -value : value;
}
