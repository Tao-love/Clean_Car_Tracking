/* line_run 状态机契约：用纯 C 桩替代硬件，只验证运行编排。 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "UART_Auto_PID.h"
#include "control_types.h"
#include "encoder.h"
#include "gray_sensor.h"
#include "line_control.h"
#include "line_run.h"
#include "motor.h"
#include "mpu6050.h"
#include "speed_pi.h"

static ControlParams gParams;
static const ControlParams *gParamsResult = &gParams;
static LineSensorSample gSensorSample;
static LineSensorSample gLastLineInput;
static int16_t gLastTurnDamping;
static uint32_t gEncoderInitCalls;
static uint32_t gEncoderUpdateCalls;
static uint32_t gMotorSetCalls;
static uint32_t gMotorStopCalls;
static uint32_t gMpuUpdateCalls;
static uint32_t gLineResetCalls;
static uint32_t gSpeedResetCalls;
static int gTestResult;

void (*const interruptVectors[])(void)
    __attribute__((used, section(".intvecs"))) = {0};

int main(void);

void _c_int00(void)
{
    gTestResult = main();
    for (;;) {
    }
}

const ControlParams *ControlParams_Get(void) { return gParamsResult; }
void Encoder_Init(void) { gEncoderInitCalls++; }
void Encoder_UpdateSpeeds(void) { gEncoderUpdateCalls++; }
int16_t Encoder_GetLeftSpeed(void) { return 3; }
int16_t Encoder_GetRightSpeed(void) { return 4; }
LineSensorSample LineSensor_ReadSample(void) { return gSensorSample; }
void Motor_SetSpeed(int16_t leftPwm, int16_t rightPwm)
{
    (void) leftPwm;
    (void) rightPwm;
    gMotorSetCalls++;
}
void Motor_Stop(void) { gMotorStopCalls++; }
void MPU6050_Update(void) { gMpuUpdateCalls++; }
int16_t MPU6050_GetTurnDamping(void) { return 17; }

void LineControl_Reset(LineControlState *state)
{
    (void) state;
    gLineResetCalls++;
}

LineControlOutput LineControl_Step(LineControlState *state,
    const LineSensorSample *sample, const ControlParams *params,
    int16_t turnDamping)
{
    LineControlOutput output = {11, 12, 1, false};
    (void) state;
    (void) params;
    gLastTurnDamping = turnDamping;
    gLastLineInput = *sample;
    return output;
}

void SpeedPI_Reset(SpeedPIState *state)
{
    (void) state;
    gSpeedResetCalls++;
}

SpeedPIOutput SpeedPI_Step(SpeedPIState *state, int16_t target,
    int16_t actual, int32_t kpQ16, int32_t kiQ16, int32_t feedforwardQ16,
    int32_t integralLimit, int16_t pwmLimit)
{
    SpeedPIOutput output = {target, false};
    (void) state;
    (void) actual;
    (void) kpQ16;
    (void) kiQ16;
    (void) feedforwardQ16;
    (void) integralLimit;
    (void) pwmLimit;
    return output;
}

static void Test_ResetCounters(void)
{
    gEncoderInitCalls = 0U;
    gEncoderUpdateCalls = 0U;
    gMotorSetCalls = 0U;
    gMotorStopCalls = 0U;
    gLineResetCalls = 0U;
    gSpeedResetCalls = 0U;
}

static int Test_DurationAndStartStop(void)
{
    uint16_t tick;

    Test_ResetCounters();
    if (!LineRun_Init() || LineRun_IsRunning() ||
        (gEncoderInitCalls != 1U) || (gMotorStopCalls != 1U)) {
        return 1;
    }
    if (!LineRun_Start() || !LineRun_IsRunning() || LineRun_Start()) {
        return 2;
    }
    gSensorSample.valid = true;
    gSensorSample.error = 10;
    for (tick = 0U; tick < (LINE_RUN_DURATION_TICKS - 1U); tick++) {
        LineRun_ControlTick();
    }
    if (!LineRun_IsRunning() ||
        (gMotorSetCalls != (LINE_RUN_DURATION_TICKS - 1U))) {
        return 3;
    }
    LineRun_ControlTick();
    if (LineRun_IsRunning() ||
        (gMotorSetCalls != LINE_RUN_DURATION_TICKS) ||
        (gMotorStopCalls != 2U)) {
        return 4;
    }
    LineRun_ControlTick();
    if (gEncoderUpdateCalls != LINE_RUN_DURATION_TICKS) {
        return 5;
    }
    return 0;
}

static int Test_InvalidLineKeepsLastError(void)
{
    if (!LineRun_Start()) {
        return 10;
    }
    gSensorSample.valid = true;
    gSensorSample.error = 321;
    LineRun_ControlTick();
    gSensorSample.valid = false;
    gSensorSample.error = -999;
    LineRun_ControlTick();
    if (!gLastLineInput.valid || (gLastLineInput.error != 321)) {
        return 11;
    }
    LineRun_Stop();
    if (LineRun_IsRunning()) {
        return 12;
    }
    if (!LineRun_Start()) {
        return 13;
    }
    gSensorSample.valid = false;
    gSensorSample.error = 777;
    LineRun_ControlTick();
    if (!gLastLineInput.valid || (gLastLineInput.error != 0)) {
        return 14;
    }
    return 0;
}

static int Test_StopClearsControlState(void)
{
    uint32_t lineResets = gLineResetCalls;
    uint32_t speedResets = gSpeedResetCalls;

    LineRun_Stop();
    if (LineRun_IsRunning() || (gLineResetCalls != (lineResets + 1U)) ||
        (gSpeedResetCalls != (speedResets + 2U))) {
        return 20;
    }
    if (!LineRun_Start()) {
        return 21;
    }
    gSensorSample.valid = false;
    LineRun_ControlTick();
    if (gLastLineInput.error != 0) {
        return 22;
    }
    if (gLastTurnDamping != 17) {
        return 23;
    }
    if (gMpuUpdateCalls == 0U) {
        return 24;
    }
    return 0;
}

static uint8_t Test_Xor(const char *text)
{
    uint8_t checksum = 0U;

    while (*text != '\0') {
        checksum ^= (uint8_t) *text;
        text++;
    }
    return checksum;
}

static void Test_Frame(char *destination, size_t destinationSize,
    const char *body)
{
    (void) snprintf(destination, destinationSize, "%s*%02X", body,
        (unsigned int) Test_Xor(body));
}

static void Test_RequestControlReset(void *context)
{
    (void) context;
    LineRun_ResetForTuningUpdate();
}

static int Test_AcceptedTuningUpdateResetsControlState(void)
{
    UARTAutoPidConfig config = {&gParams, Test_RequestControlReset, 0};
    uint32_t lineResets;
    uint32_t speedResets;
    char line[160];

    LineRun_Stop();
    if (!LineRun_Start()) {
        return 30;
    }
    lineResets = gLineResetCalls;
    speedResets = gSpeedResetCalls;
    UART_Auto_PID_Init(&config);
    Test_Frame(line, sizeof(line),
        "SETALL SEQ:1 LKP:1 LKI:0 RKP:1 RKI:0 LINEP:0.5 LINED:0.25");
    if (!UART_Auto_PID_ProcessLineForTest(line) ||
        (gLineResetCalls != (lineResets + 1U)) ||
        (gSpeedResetCalls != (speedResets + 2U))) {
        return 31;
    }
    return 0;
}

int main(void)
{
    int result;

    gParams.maxPwm = 300;
    result = Test_DurationAndStartStop();
    if (result != 0) {
        return result;
    }
    result = Test_InvalidLineKeepsLastError();
    if (result != 0) {
        return result;
    }
    result = Test_StopClearsControlState();
    if (result != 0) {
        return result;
    }
    result = Test_AcceptedTuningUpdateResetsControlState();
    if (result != 0) {
        return result;
    }
    LineRun_Stop();
    gParamsResult = 0;
    if (LineRun_Init()) {
        return 30;
    }
    return 0;
}

/* Link the DriverLib-free protocol core into this composition contract. */
#include "../../src/UART_Auto_PID.c"
