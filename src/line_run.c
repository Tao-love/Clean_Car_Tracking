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
#include "speed_pi.h"

static const ControlParams *gParams;
static LineControlState gLineState;
static SpeedPIState gLeftPiState;
static SpeedPIState gRightPiState;
static uint16_t gRunTicks;
static int16_t gLastLineError;
static bool gHasLastLineError;
static bool gRunning;
static bool gBenchRunning;
static int16_t gBenchTarget;

#define LINE_RUN_BENCH_DEFAULT_TARGET (5)

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
}

bool LineRun_Init(void)
{
    gParams = ControlParams_Get();
    Encoder_Init();
    gRunning = false;
    gBenchRunning = false;
    gBenchTarget = LINE_RUN_BENCH_DEFAULT_TARGET;
    LineRun_ResetControlState();
    Motor_Stop();
    return gParams != 0;
}

bool LineRun_Start(void)
{
    if (gRunning || (gParams == 0)) {
        return false;
    }
    LineRun_ResetControlState();
    gBenchRunning = false;
    gBenchTarget = LINE_RUN_BENCH_DEFAULT_TARGET;
    gRunning = true;
    return true;
}

bool LineRun_StartBench(void)
{
    if (gRunning || (gParams == 0)) {
        return false;
    }
    LineRun_ResetControlState();
    gBenchTarget = LINE_RUN_BENCH_DEFAULT_TARGET;
    gBenchRunning = true;
    gRunning = true;
    return true;
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
    gRunning = false;
    gBenchRunning = false;
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
    if (gBenchRunning) {
        line.error = 0;
        lineOutput.leftTarget = gBenchTarget;
        lineOutput.rightTarget = gBenchTarget;
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
