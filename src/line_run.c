/* 150 Hz 循线、速度 PI 与 10 秒运行上限。 */

#include <stdbool.h>
#include <stdint.h>

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

static void LineRun_ResetControlState(void)
{
    LineControl_Reset(&gLineState);
    SpeedPI_Reset(&gLeftPiState);
    SpeedPI_Reset(&gRightPiState);
    gRunTicks = 0U;
    gLastLineError = 0;
    gHasLastLineError = false;
}

bool LineRun_Init(void)
{
    gParams = ControlParams_Get();
    Encoder_Init();
    gRunning = false;
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
    gRunning = true;
    return true;
}

void LineRun_Stop(void)
{
    Motor_Stop();
    gRunning = false;
    LineRun_ResetControlState();
}

void LineRun_ControlTick(void)
{
    LineSensorSample line;
    LineControlOutput lineOutput;
    SpeedPIOutput leftOutput;
    SpeedPIOutput rightOutput;

    if (!gRunning) {
        return;
    }

    Encoder_UpdateSpeeds();
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
    leftOutput = SpeedPI_Step(&gLeftPiState,
        lineOutput.leftTarget, Encoder_GetLeftSpeed(),
        gParams->speedKpLeftQ16, gParams->speedKiLeftQ16,
        gParams->speedFeedforwardLeftQ16, gParams->speedIntegralLimit,
        gParams->maxPwm);
    rightOutput = SpeedPI_Step(&gRightPiState,
        lineOutput.rightTarget, Encoder_GetRightSpeed(),
        gParams->speedKpRightQ16, gParams->speedKiRightQ16,
        gParams->speedFeedforwardRightQ16, gParams->speedIntegralLimit,
        gParams->maxPwm);
    Motor_SetSpeed(leftOutput.pwm, rightOutput.pwm);

    gRunTicks++;
    if (gRunTicks >= LINE_RUN_DURATION_TICKS) {
        LineRun_Stop();
    }
}

bool LineRun_IsRunning(void)
{
    return gRunning;
}
