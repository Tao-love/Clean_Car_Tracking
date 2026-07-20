#include <stdint.h>

#include "pid.h"

#define PID_KP (18)
#define PID_KI (0)
#define PID_KD (8)
#define PID_SCALE (100)
#define PID_INTEGRAL_LIMIT (10000)

static int16_t gLastError = 0;
static int32_t gIntegral = 0;

void PID_Init(void)
{
    gLastError = 0;
    gIntegral = 0;
}

int16_t PID_Calculate(int16_t error)
{
    int16_t derivative;
    int32_t output;

    gIntegral += error;
    if (gIntegral > PID_INTEGRAL_LIMIT) {
        gIntegral = PID_INTEGRAL_LIMIT;
    } else if (gIntegral < -PID_INTEGRAL_LIMIT) {
        gIntegral = -PID_INTEGRAL_LIMIT;
    }

    derivative = error - gLastError;
    gLastError = error;

    output = ((int32_t) PID_KP * error) +
             ((int32_t) PID_KI * gIntegral) +
             ((int32_t) PID_KD * derivative);

    return (int16_t) (output / PID_SCALE);
}
