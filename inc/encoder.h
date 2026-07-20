// ----- AI
/*
 * MG310 左右 AB 相编码器模块。
 * 职责：A 相双边沿中断结合 B 相判向，并每 10 ms 计算原始/平滑速度。
 * 依赖：SysConfig GPIO_ENCODER。上下文：GROUP1 ISR 只更新计数，速度更新在主循环 100 Hz 任务。
 * 单位：count 是 A 相双边沿计数，speed 是每 10 ms 计数。硬件副作用：开启 GROUP1 NVIC。
 * 故障输出：速度差值超过 int16 时饱和；方向反向由编译期常量标定。
 */

#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

/* 用途：清计数/滤波状态、清中断标志并开启 GROUP1 IRQ；禁止 ISR 调用。 */
void Encoder_Init(void);

/* 用途：快照计数并计算一次 10 ms 速度与 0.3/0.7 平滑值；禁止 ISR 调用。 */
void Encoder_UpdateSpeeds(void);

/* 用途：返回左/右累计计数的 32 位原子快照；无硬件副作用。 */
int32_t Encoder_GetLeftCount(void);
int32_t Encoder_GetRightCount(void);

/* 用途：返回左/右平滑速度；单位为每 10 ms 计数。 */
int16_t Encoder_GetLeftSpeed(void);
int16_t Encoder_GetRightSpeed(void);

/* 用途：返回左/右未滤波速度；单位为每 10 ms 计数。 */
int16_t Encoder_GetLeftRawSpeed(void);
int16_t Encoder_GetRightRawSpeed(void);

#endif /* ENCODER_H */
// ----- AI
