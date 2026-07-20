// ----- AI
/*
 * 八路数字灰度循线 PD 模块。
 * 职责：对有效位置误差做限幅差分和一阶低通，计算左右目标速度。
 * 依赖：LineSensorSample、ControlParams。上下文：100 Hz 主循环任务。
 * 单位：error=-3500..3500，输出速度为每 10 ms 编码器计数。
 * 硬件副作用：无。故障输出：sensorValid=false 交由 safety_guard 计时。
 */

#ifndef LINE_CONTROL_H
#define LINE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "autotune_types.h"

typedef struct {
    int16_t previousError;
    int32_t filteredDerivative;
    bool initialized;
} LineControlState;

typedef struct {
    int16_t leftTarget;
    int16_t rightTarget;
    int16_t deltaSpeed;
    bool targetSaturated;
} LineControlOutput;

/* 用途：清空差分滤波状态；无硬件副作用，ISR 不可调用。 */
void LineControl_Reset(LineControlState *state);

/* 用途：执行一次 PD 和分层限幅；无效灰度时输出零速并不更新旧误差。 */
LineControlOutput LineControl_Step(LineControlState *state,
    const LineSensorSample *sample, const ControlParams *params);

#endif /* LINE_CONTROL_H */
// ----- AI
