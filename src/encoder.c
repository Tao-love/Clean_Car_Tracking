#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "encoder.h"

#define ENCODER_FILTER_PREV_WEIGHT (3)
#define ENCODER_FILTER_CURR_WEIGHT (7)
#define ENCODER_FILTER_SCALE       (10)

#define ENCODER_LEFT_REVERSE       (0)
#define ENCODER_RIGHT_REVERSE      (0)

static volatile int32_t gLeftCount = 0;
static volatile int32_t gRightCount = 0;
static int32_t gLastLeftCount = 0;
static int32_t gLastRightCount = 0;
static int16_t gLeftRawSpeed = 0;
static int16_t gRightRawSpeed = 0;
static int16_t gLeftSpeed = 0;
static int16_t gRightSpeed = 0;
static bool gLeftSpeedInitialized = false;
static bool gRightSpeedInitialized = false;

static void Encoder_HandleLeftEdge(void);
static void Encoder_HandleRightEdge(void);
static int16_t Encoder_SmoothSpeed(
    int16_t currentSpeed, int16_t lastSpeed, bool *initialized);
static int16_t Encoder_ClampDelta(int32_t delta);

void Encoder_Init(void)
{
    gLeftCount = 0;
    gRightCount = 0;
    gLastLeftCount = 0;
    gLastRightCount = 0;
    gLeftRawSpeed = 0;
    gRightRawSpeed = 0;
    gLeftSpeed = 0;
    gRightSpeed = 0;
    gLeftSpeedInitialized = false;
    gRightSpeedInitialized = false;

    DL_GPIO_clearInterruptStatus(GPIO_ENCODER_PORT,
        GPIO_ENCODER_ENCODER_LEFT_A_PIN | GPIO_ENCODER_ENCODER_RIGHT_A_PIN);
    NVIC_EnableIRQ(GPIO_ENCODER_INT_IRQN);
}

void Encoder_UpdateSpeeds(void)
{
    int32_t leftCount;
    int32_t rightCount;
    int32_t leftDelta;
    int32_t rightDelta;

    NVIC_DisableIRQ(GPIO_ENCODER_INT_IRQN);
    leftCount = gLeftCount;
    rightCount = gRightCount;
    NVIC_EnableIRQ(GPIO_ENCODER_INT_IRQN);

    leftDelta = leftCount - gLastLeftCount;
    rightDelta = rightCount - gLastRightCount;
    gLastLeftCount = leftCount;
    gLastRightCount = rightCount;

    gLeftRawSpeed = Encoder_ClampDelta(leftDelta);
    gRightRawSpeed = Encoder_ClampDelta(rightDelta);
    gLeftSpeed = Encoder_SmoothSpeed(
        gLeftRawSpeed, gLeftSpeed, &gLeftSpeedInitialized);
    gRightSpeed = Encoder_SmoothSpeed(
        gRightRawSpeed, gRightSpeed, &gRightSpeedInitialized);
}

int32_t Encoder_GetLeftCount(void)
{
    return gLeftCount;
}

int32_t Encoder_GetRightCount(void)
{
    return gRightCount;
}

int16_t Encoder_GetLeftSpeed(void)
{
    return gLeftSpeed;
}

int16_t Encoder_GetRightSpeed(void)
{
    return gRightSpeed;
}

int16_t Encoder_GetLeftRawSpeed(void)
{
    return gLeftRawSpeed;
}

int16_t Encoder_GetRightRawSpeed(void)
{
    return gRightRawSpeed;
}

void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
        case GPIO_ENCODER_INT_IIDX:
            if (DL_GPIO_getEnabledInterruptStatus(GPIO_ENCODER_PORT,
                    GPIO_ENCODER_ENCODER_LEFT_A_PIN)) {
                Encoder_HandleLeftEdge();
                DL_GPIO_clearInterruptStatus(
                    GPIO_ENCODER_PORT, GPIO_ENCODER_ENCODER_LEFT_A_PIN);
            }

            if (DL_GPIO_getEnabledInterruptStatus(GPIO_ENCODER_PORT,
                    GPIO_ENCODER_ENCODER_RIGHT_A_PIN)) {
                Encoder_HandleRightEdge();
                DL_GPIO_clearInterruptStatus(
                    GPIO_ENCODER_PORT, GPIO_ENCODER_ENCODER_RIGHT_A_PIN);
            }
            break;
        default:
            break;
    }
}

static void Encoder_HandleLeftEdge(void)
{
    bool phaseAHigh = (DL_GPIO_readPins(GPIO_ENCODER_PORT,
        GPIO_ENCODER_ENCODER_LEFT_A_PIN) != 0);
    bool phaseBHigh = (DL_GPIO_readPins(GPIO_ENCODER_PORT,
        GPIO_ENCODER_ENCODER_LEFT_B_PIN) != 0);
    int32_t step = (phaseAHigh == phaseBHigh) ? 1 : -1;

    if (ENCODER_LEFT_REVERSE) {
        step = -step;
    }
    gLeftCount += step;
}

static void Encoder_HandleRightEdge(void)
{
    bool phaseAHigh = (DL_GPIO_readPins(GPIO_ENCODER_PORT,
        GPIO_ENCODER_ENCODER_RIGHT_A_PIN) != 0);
    bool phaseBHigh = (DL_GPIO_readPins(GPIO_ENCODER_PORT,
        GPIO_ENCODER_ENCODER_RIGHT_B_PIN) != 0);
    int32_t step = (phaseAHigh == phaseBHigh) ? 1 : -1;

    if (ENCODER_RIGHT_REVERSE) {
        step = -step;
    }
    gRightCount += step;
}

static int16_t Encoder_SmoothSpeed(
    int16_t currentSpeed, int16_t lastSpeed, bool *initialized)
{
    int32_t filteredSpeed;

    if (*initialized == false) {
        *initialized = true;
        return currentSpeed;
    }

    filteredSpeed =
        ((int32_t) lastSpeed * ENCODER_FILTER_PREV_WEIGHT) +
        ((int32_t) currentSpeed * ENCODER_FILTER_CURR_WEIGHT);
    filteredSpeed /= ENCODER_FILTER_SCALE;

    return (int16_t) filteredSpeed;
}

static int16_t Encoder_ClampDelta(int32_t delta)
{
    if (delta > INT16_MAX) {
        return INT16_MAX;
    }
    if (delta < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) delta;
}
