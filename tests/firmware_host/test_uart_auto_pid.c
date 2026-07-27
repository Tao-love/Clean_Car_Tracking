/* UART 自动调参协议核心的原生主机契约测试。 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "UART_Auto_PID.h"

static uint32_t gResetRequests;
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
    char line[160];

    gResetRequests = 0U;
    UART_Auto_PID_Init(&config);
    Test_Frame(line, sizeof(line),
        "SETALL SEQ:17 LKP:1.500000 LKI:0.250000 RKP:2.750000 "
        "RKI:0.125000 LINEP:0.500000 LINED:0.062500");
    if (!UART_Auto_PID_ProcessLineForTest(line)) {
        return 1;
    }
    if ((params.speedKpLeftQ16 != (3L * Q16_ONE / 2)) ||
        (params.speedKiLeftQ16 != (Q16_ONE / 4)) ||
        (params.speedKpRightQ16 != (11L * Q16_ONE / 4)) ||
        (params.speedKiRightQ16 != (Q16_ONE / 8)) ||
        (params.lineKpQ16 != (Q16_ONE / 2)) ||
        (params.lineKdQ16 != (Q16_ONE / 16)) ||
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
    return Test_TelemetryIsThrottled();
}
