/* UART 自动调参协议核心的原生主机契约测试。 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "UART_Auto_PID.h"

static uint32_t gResetRequests;
static uint32_t gRunStartRequests;
static bool gRunActive;
static bool gRunStartAllowed;
static bool gBenchActive;
static int16_t gBenchTarget;
static volatile int gTestResult;

void (*const interruptVectors[])(void)
    __attribute__((used, section(".intvecs"))) = {0};

int main(void);

void _c_int00(void)
{
    gTestResult = main();
    for (;;) {
    }
}

static void Test_RequestReset(void *context)
{
    (void) context;
    gResetRequests++;
}

bool LineRun_IsRunning(void)
{
    return gRunActive;
}

bool LineRun_Start(void)
{
    gRunStartRequests++;
    if (!gRunStartAllowed) {
        return false;
    }
    gRunActive = true;
    return true;
}

bool LineRun_StartBench(void)
{
    gRunStartRequests++;
    if (!gRunStartAllowed) {
        return false;
    }
    gRunActive = true;
    gBenchActive = true;
    gBenchTarget = 5;
    return true;
}

bool LineRun_IsBenchRunning(void)
{
    return gRunActive && gBenchActive;
}

bool LineRun_SetBenchTarget(int16_t target)
{
    if (!LineRun_IsBenchRunning() || (target < 1) || (target > 8)) {
        return false;
    }
    gBenchTarget = target;
    return true;
}

int32_t Record_GetLeft(void) { return 0; }
int32_t Record_GetRight(void) { return 0; }

static ControlParams Test_DefaultParams(void)
{
    ControlParams params = {
        12L * Q16_ONE, 0, 42L * Q16_ONE,
        12L * Q16_ONE, 0, (69L * Q16_ONE / 2),
        (Q16_ONE / 70), (Q16_ONE / 320),
        (5L * Q16_ONE / 8), 10000,
        27, 67, 67, 300, 2333
    };

    return params;
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

static int Test_AcceptsCompleteSetAll(void)
{
    ControlParams params = Test_DefaultParams();
    UARTAutoPidConfig config = {&params, Test_RequestReset, 0};
    char line[300];

    gResetRequests = 0U;
    UART_Auto_PID_Init(&config);
    Test_Frame(line, sizeof(line),
        "SETALL SEQ:17 LKP:1.500000 LKI:0.250000 LFF:4.250000 "
        "RKP:2.750000 RKI:0.125000 RFF:3.500000 LINEP:0.500000 "
        "LINED:0.062500 ALPHA:0.500000 ILIMIT:20000 BASE:15 "
        "TARGET:67 DELTA:35 PWM:1000 DLIMIT:2000");
    if (!UART_Auto_PID_ProcessLineForTest(line)) {
        return 1;
    }
    if ((params.speedKpLeftQ16 != (3L * Q16_ONE / 2)) ||
        (params.speedKiLeftQ16 != (Q16_ONE / 4)) ||
        (params.speedFeedforwardLeftQ16 != (17L * Q16_ONE / 4)) ||
        (params.speedKpRightQ16 != (11L * Q16_ONE / 4)) ||
        (params.speedKiRightQ16 != (Q16_ONE / 8)) ||
        (params.speedFeedforwardRightQ16 != (7L * Q16_ONE / 2)) ||
        (params.lineKpQ16 != (Q16_ONE / 2)) ||
        (params.lineKdQ16 != (Q16_ONE / 16)) ||
        (params.derivativeAlphaQ16 != (Q16_ONE / 2)) ||
        (params.speedIntegralLimit != 20000) ||
        (params.baseSpeed != 15) ||
        (params.maxTargetSpeed != 67) ||
        (params.maxDeltaSpeed != 35) ||
        (params.maxPwm != 1000) ||
        (params.derivativeLimit != 2000) ||
        (gResetRequests != 1U)) {
        return 2;
    }
    return 0;
}

static int Test_InvalidFramesAreAtomic(void)
{
    ControlParams params = Test_DefaultParams();
    ControlParams unchanged = params;
    UARTAutoPidConfig config = {&params, Test_RequestReset, 0};
    char line[180];
    char longLine[UART_AUTO_PID_MAX_LINE + 2U];
    size_t index;

    gResetRequests = 0U;
    UART_Auto_PID_Init(&config);

    Test_Frame(line, sizeof(line),
        "SETALL SEQ:1 LKP:1 LKI:0 RKP:1 RKI:0 LINEP:0.5 LINED:0.25");
    line[strlen(line) - 1U] = (line[strlen(line) - 1U] == '0') ? '1' : '0';
    if (UART_Auto_PID_ProcessLineForTest(line) ||
        (memcmp(&params, &unchanged, sizeof(params)) != 0)) {
        return 10;
    }

    Test_Frame(line, sizeof(line),
        "SETALL SEQ:1 LKP:1 LKP:2 LKI:0 RKP:1 RKI:0 LINEP:0.5 LINED:0.25");
    if (UART_Auto_PID_ProcessLineForTest(line) ||
        (memcmp(&params, &unchanged, sizeof(params)) != 0)) {
        return 11;
    }

    Test_Frame(line, sizeof(line),
        "SETALL SEQ:1 LKP:1 LKI:0 RKP:1 RKI:0 LINEP:0.5");
    if (UART_Auto_PID_ProcessLineForTest(line) ||
        (memcmp(&params, &unchanged, sizeof(params)) != 0)) {
        return 12;
    }

    Test_Frame(line, sizeof(line),
        "SETALL SEQ:1 LKP:64.1 LKI:0 RKP:1 RKI:0 LINEP:0.5 LINED:0.25");
    if (UART_Auto_PID_ProcessLineForTest(line) ||
        (memcmp(&params, &unchanged, sizeof(params)) != 0)) {
        return 13;
    }

    for (index = 0U; index < (sizeof(longLine) - 1U); index++) {
        longLine[index] = 'A';
    }
    longLine[index] = '\0';
    if (UART_Auto_PID_ProcessLineForTest(longLine) ||
        (memcmp(&params, &unchanged, sizeof(params)) != 0) ||
        (gResetRequests != 0U)) {
        return 14;
    }

    Test_Frame(line, sizeof(line),
        "SETALL SEQ:2 LKP:1 LKI:0 LFF:1 RKP:1 RKI:0 RFF:1 "
        "LINEP:0 LINED:0 ALPHA:0.625 ILIMIT:10000 BASE:15 "
        "TARGET:67 DELTA:35 PWM:1001 DLIMIT:2333");
    if (UART_Auto_PID_ProcessLineForTest(line) ||
        (memcmp(&params, &unchanged, sizeof(params)) != 0) ||
        (gResetRequests != 0U)) {
        return 15;
    }
    return 0;
}

static int Test_RejectsUpdateWithoutResetCallback(void)
{
    ControlParams params = Test_DefaultParams();
    ControlParams unchanged = params;
    UARTAutoPidConfig config = {&params, 0, 0};
    char line[160];

    UART_Auto_PID_Init(&config);
    Test_Frame(line, sizeof(line),
        "SETALL SEQ:9 LKP:1.5 LKI:0.25 RKP:2.75 RKI:0.125 "
        "LINEP:0.5 LINED:0.0625");
    if (UART_Auto_PID_ProcessLineForTest(line) ||
        (memcmp(&params, &unchanged, sizeof(params)) != 0)) {
        return 15;
    }
    return 0;
}

static int Test_TelemetryIsThrottled(void)
{
    ControlParams params = Test_DefaultParams();
    UARTAutoPidConfig config = {&params, Test_RequestReset, 0};
    UARTAutoPidControlSample sample = {0};

    UART_Auto_PID_Init(&config);
    UART_Auto_PID_OnControlSample(&sample);
    if (UART_Auto_PID_IsTelemetryReadyForTest()) {
        return 20;
    }
    UART_Auto_PID_OnControlSample(&sample);
    if (UART_Auto_PID_IsTelemetryReadyForTest()) {
        return 21;
    }
    UART_Auto_PID_OnControlSample(&sample);
    if (!UART_Auto_PID_IsTelemetryReadyForTest()) {
        return 22;
    }
    UART_Auto_PID_OnControlSample(&sample);
    return UART_Auto_PID_IsTelemetryReadyForTest() ? 23 : 0;
}

static int Test_RunCommandRequiresProtectedIdleStart(void)
{
    ControlParams params = Test_DefaultParams();
    ControlParams unchanged = params;
    UARTAutoPidConfig config = {&params, Test_RequestReset, 0};
    char line[40];

    gResetRequests = 0U;
    gRunStartRequests = 0U;
    gRunActive = false;
    gRunStartAllowed = true;
    UART_Auto_PID_Init(&config);

    Test_Frame(line, sizeof(line), "RUN SEQ:18");
    if (!UART_Auto_PID_ProcessLineForTest(line) ||
        (gRunStartRequests != 1U) || !gRunActive ||
        (memcmp(&params, &unchanged, sizeof(params)) != 0) ||
        (gResetRequests != 0U)) {
        return 30;
    }

    gRunActive = false;
    line[strlen(line) - 1U] = (line[strlen(line) - 1U] == '0') ? '1' : '0';
    if (UART_Auto_PID_ProcessLineForTest(line) ||
        (gRunStartRequests != 1U) || gRunActive) {
        return 31;
    }

    Test_Frame(line, sizeof(line), "RUN");
    if (UART_Auto_PID_ProcessLineForTest(line) ||
        (gRunStartRequests != 1U) || gRunActive) {
        return 32;
    }

    gRunActive = true;
    Test_Frame(line, sizeof(line), "RUN SEQ:19");
    if (UART_Auto_PID_ProcessLineForTest(line) ||
        (gRunStartRequests != 1U)) {
        return 33;
    }

    gRunActive = false;
    gRunStartAllowed = false;
    Test_Frame(line, sizeof(line), "RUN SEQ:20");
    if (UART_Auto_PID_ProcessLineForTest(line) ||
        (gRunStartRequests != 2U) || gRunActive) {
        return 34;
    }
    return 0;
}

static int Test_BenchCommandsRequireBenchState(void)
{
    ControlParams params = Test_DefaultParams();
    UARTAutoPidConfig config = {&params, Test_RequestReset, 0};
    char line[56];

    gRunStartRequests = 0U;
    gRunActive = false;
    gBenchActive = false;
    gRunStartAllowed = true;
    UART_Auto_PID_Init(&config);
    Test_Frame(line, sizeof(line), "BENCH SEQ:41");
    if (!UART_Auto_PID_ProcessLineForTest(line) || !gBenchActive ||
        (gBenchTarget != 5) || (gRunStartRequests != 1U)) {
        return 40;
    }
    Test_Frame(line, sizeof(line), "BENCHTARGET SEQ:42 VALUE:8");
    if (!UART_Auto_PID_ProcessLineForTest(line) || (gBenchTarget != 8)) {
        return 41;
    }
    gRunActive = false;
    gBenchActive = false;
    Test_Frame(line, sizeof(line), "BENCHTARGET SEQ:43 VALUE:5");
    if (UART_Auto_PID_ProcessLineForTest(line) || (gBenchTarget != 8)) {
        return 42;
    }
    return 0;
}

int main(void)
{
    int result = Test_AcceptsCompleteSetAll();

    if (result != 0) {
        return result;
    }
    result = Test_InvalidFramesAreAtomic();
    if (result != 0) {
        return result;
    }
    result = Test_RejectsUpdateWithoutResetCallback();
    if (result != 0) {
        return result;
    }
    result = Test_TelemetryIsThrottled();
    if (result != 0) {
        return result;
    }
    result = Test_RunCommandRequiresProtectedIdleStart();
    if (result != 0) {
        return result;
    }
    return Test_BenchCommandsRequireBenchState();
}
