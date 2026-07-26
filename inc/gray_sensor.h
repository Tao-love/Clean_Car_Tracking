// ----- AI
/*
 * 八路数字灰度采样模块。
 * 职责：同一次函数调用读取 8 个 GPIO，生成位图、激活数和加权中心误差。
 * 依赖：SysConfig GPIO_LINE。上下文：150 Hz 主循环任务。
 * 硬件副作用：只读 GPIO。故障输出：全未触发时 valid=false，不沿用旧误差。
 */

#ifndef GRAY_SENSOR_H
#define GRAY_SENSOR_H

#include <stdint.h>

#include "autotune_types.h"

#define LINE_SENSOR_COUNT      (8U)
#define LINE_SENSOR_ACTIVE_LOW (0)

/* 用途：返回 D0..D7 对应 bit0..bit7 的原始激活位图；只读 GPIO，ISR 不可调用。 */
uint8_t LineSensor_ReadRaw(void);

/* 用途：读一次位图并计算完整样本；返回的 error 范围 -3500..3500。 */
LineSensorSample LineSensor_ReadSample(void);

#endif /* GRAY_SENSOR_H */
// ----- AI
