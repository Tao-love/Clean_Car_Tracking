// ----- AI
/*
 * 八路数字灰度循线 PD 模块。
 * 职责：对有效位置误差做限幅差分和一阶低通，计算左右目标速度。
 * 依赖：LineSensorSample、ControlParams。上下文：150 Hz 主循环任务。
 * 单位：error=-3500..3500，输出速度为每 6.667 ms 编码器计数。
 * 硬件副作用：无。无效灰度由 line_run 转换为最后一次有效误差。
 */

#ifndef LINE_CONTROL_H
#define LINE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "control_types.h"

typedef struct {
    /* 上一次灰度位置误差，供下一次计算 D 项使用。 */
    int16_t previousError;
    /* 平滑后的 D 项历史值；不是要手动调的速度参数。 */
    int32_t filteredDerivative;
    /* 上一周期实际输出的左右差速，用于让转弯指令平滑变化。 */
    int16_t previousDeltaSpeed;
    /* false 表示还没有上一轮数据，因此第一轮 D 项为 0。 */
    bool initialized;
} LineControlState;

typedef struct {
    /* 返回给左、右速度 PI 的目标速度，单位为“每 6.667 ms 编码器计数”，不是 PWM。 */
    int16_t leftTarget;
    int16_t rightTarget;
    /* 左右目标速度的差值：正值时左慢右快，负值时左快右慢。 */
    int16_t deltaSpeed;
    /* true 表示左右至少一侧的目标速度超过上限并被限制。 */
    bool targetSaturated;
} LineControlOutput;

/* 用途：清空差分滤波状态；无硬件副作用，ISR 不可调用。 */
void LineControl_Reset(LineControlState *state);

/*
 * 用途：执行一次循线 PD 和限幅。
 * 输出：LineControlOutput；leftTarget/rightTarget 是下一步应交给速度 PI 的目标速度。
 *       无效灰度时返回全零，且不更新旧误差。
 */
LineControlOutput LineControl_Step(LineControlState *state,
    const LineSensorSample *sample, const ControlParams *params,
    int16_t turnDamping);

#endif /* LINE_CONTROL_H */
// ----- AI
