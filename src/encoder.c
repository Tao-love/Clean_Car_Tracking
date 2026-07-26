// ----- AI
/* MG310 AB 相编码器计数、判向与 150 Hz 速度滤波。 */

#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "encoder.h"

#define ENCODER_FILTER_PREV_WEIGHT (3)
#define ENCODER_FILTER_CURR_WEIGHT (7)
#define ENCODER_FILTER_SCALE       (10)

/* 悬空测试若发现正向 PWM 得到负速度，只修改对应常量为 1。 */
#define ENCODER_LEFT_REVERSE  (0)
#define ENCODER_RIGHT_REVERSE (0)

static volatile int32_t gLeftCount;
static volatile int32_t gRightCount;
static int32_t gLastLeftCount;
static int32_t gLastRightCount;
static int16_t gLeftRawSpeed;
static int16_t gRightRawSpeed;
static int16_t gLeftSpeed;
static int16_t gRightSpeed;
static bool gLeftSpeedInitialized;
static bool gRightSpeedInitialized;

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

    /* 清除左右 A 相上电残留的 GPIO 中断状态，避免初始化后虚假计数。 */
    DL_GPIO_clearInterruptStatus(GPIO_ENCODER_PORT,
        GPIO_ENCODER_ENCODER_LEFT_A_PIN | GPIO_ENCODER_ENCODER_RIGHT_A_PIN);
    /* 允许 GPIO 聚合中断 GROUP1 进入 CPU，ISR 只读 A/B 相并加减计数。 */
    NVIC_EnableIRQ(GPIO_ENCODER_INT_IRQN);
}

void Encoder_UpdateSpeeds(void)
{
    int32_t leftCount;
    int32_t rightCount;

    /* 短暂屏蔽 GROUP1 IRQ，使左右计数属于同一 6.667 ms 快照。 */
    NVIC_DisableIRQ(GPIO_ENCODER_INT_IRQN);
    leftCount = gLeftCount;
    rightCount = gRightCount;
    /* 快照完成后立即恢复 GROUP1 IRQ，边沿标志在屏蔽期仍由硬件保留。 */
    NVIC_EnableIRQ(GPIO_ENCODER_INT_IRQN);

    gLeftRawSpeed = Encoder_ClampDelta(leftCount - gLastLeftCount);
    gRightRawSpeed = Encoder_ClampDelta(rightCount - gLastRightCount);
    gLastLeftCount = leftCount;
    gLastRightCount = rightCount;
    gLeftSpeed = Encoder_SmoothSpeed(
        gLeftRawSpeed, gLeftSpeed, &gLeftSpeedInitialized);
    gRightSpeed = Encoder_SmoothSpeed(
        gRightRawSpeed, gRightSpeed, &gRightSpeedInitialized);
}

int32_t Encoder_GetLeftCount(void) { return gLeftCount; }
int32_t Encoder_GetRightCount(void) { return gRightCount; }
int16_t Encoder_GetLeftSpeed(void) { return gLeftSpeed; }
int16_t Encoder_GetRightSpeed(void) { return gRightSpeed; }
int16_t Encoder_GetLeftRawSpeed(void) { return gLeftRawSpeed; }
int16_t Encoder_GetRightRawSpeed(void) { return gRightRawSpeed; }

void GROUP1_IRQHandler(void)
{
    /* 读取中断聚合器 GROUP1 当前最高优先级来源。 */
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
        case GPIO_ENCODER_INT_IIDX:
            /* 只检查已启用的左 A 相边沿标志。 */
            if (DL_GPIO_getEnabledInterruptStatus(GPIO_ENCODER_PORT,
                    GPIO_ENCODER_ENCODER_LEFT_A_PIN) != 0U) {
                Encoder_HandleLeftEdge();
                /* 左边沿计数后清对应硬件标志，允许下一边沿再次触发。 */
                DL_GPIO_clearInterruptStatus(
                    GPIO_ENCODER_PORT, GPIO_ENCODER_ENCODER_LEFT_A_PIN);
            }
            /* 只检查已启用的右 A 相边沿标志。 */
            if (DL_GPIO_getEnabledInterruptStatus(GPIO_ENCODER_PORT,
                    GPIO_ENCODER_ENCODER_RIGHT_A_PIN) != 0U) {
                Encoder_HandleRightEdge();
                /* 右边沿计数后清对应硬件标志。 */
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
    /* 读左 A 相当前电平，与 B 相关系决定方向。 */
    bool phaseAHigh = DL_GPIO_readPins(GPIO_ENCODER_PORT,
        GPIO_ENCODER_ENCODER_LEFT_A_PIN) != 0U;
    /* 读左 B 相当前电平，B 相不开中断。 */
    bool phaseBHigh = DL_GPIO_readPins(GPIO_ENCODER_PORT,
        GPIO_ENCODER_ENCODER_LEFT_B_PIN) != 0U;
    int32_t step = (phaseAHigh == phaseBHigh) ? 1 : -1;
    if (ENCODER_LEFT_REVERSE) { step = -step; }
    gLeftCount += step;
}

static void Encoder_HandleRightEdge(void)
{
    /* 读右 A 相当前电平，与 B 相关系决定方向。 */
    bool phaseAHigh = DL_GPIO_readPins(GPIO_ENCODER_PORT,
        GPIO_ENCODER_ENCODER_RIGHT_A_PIN) != 0U;
    /* 读右 B 相当前电平，B 相不开中断。 */
    bool phaseBHigh = DL_GPIO_readPins(GPIO_ENCODER_PORT,
        GPIO_ENCODER_ENCODER_RIGHT_B_PIN) != 0U;
    int32_t step = (phaseAHigh == phaseBHigh) ? 1 : -1;
    if (ENCODER_RIGHT_REVERSE) { step = -step; }
    gRightCount += step;
}

static int16_t Encoder_SmoothSpeed(
    int16_t currentSpeed, int16_t lastSpeed, bool *initialized)
{
    if (!*initialized) {
        *initialized = true;
        return currentSpeed;
    }
    return (int16_t) ((((int32_t) lastSpeed * ENCODER_FILTER_PREV_WEIGHT) +
        ((int32_t) currentSpeed * ENCODER_FILTER_CURR_WEIGHT)) /
        ENCODER_FILTER_SCALE);
}

static int16_t Encoder_ClampDelta(int32_t delta)
{
    if (delta > INT16_MAX) { return INT16_MAX; }
    if (delta < INT16_MIN) { return INT16_MIN; }
    return (int16_t) delta;
}
// ----- AI
