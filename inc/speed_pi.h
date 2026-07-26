// ----- AI
/*
 * 单轮速度 PI 模块。
 * 职责：用 Q16.16 比例、积分和前馈计算 PWM，并执行条件积分抗饱和。
 * 依赖：ControlParams 中一侧轮子的参数。上下文：150 Hz 主循环任务。
 * 单位：target/actual 为每 6.667 ms 编码器计数，输出为 TB6612 PWM 比较值。
 * 硬件副作用：无。故障输出：saturated 标志。
 */

#ifndef SPEED_PI_H
#define SPEED_PI_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /* I 项的历史累计值；不是 PWM，也不是需要直接手动填写的参数。 */
    int32_t integral;
} SpeedPIState;

typedef struct {
    /* 本次返回给电机驱动模块的 PWM 命令；正负号表示方向。 */
    int16_t pwm;
    /* true：原始计算值超过 pwmLimit，pwm 已被限制在上限或下限。 */
    bool saturated;
} SpeedPIOutput;

/* 用途：清空积分状态；输入 state 必须有效；无硬件副作用，ISR 不可调用。 */
void SpeedPI_Reset(SpeedPIState *state);

/*
 * 用途：执行一次 6.667 ms PI 计算。
 * 输入：状态、目标/实测速度、Q16.16 kp/ki/feedforward、积分限值和 PWM 限值。
 * 输出：SpeedPIOutput：pwm 是实际要输出的电机 PWM；saturated 表示是否限幅。
 *       目标为 0 时清积分并返回 {0, false}。ISR 不可调用。
 */
SpeedPIOutput SpeedPI_Step(SpeedPIState *state, int16_t target,
    int16_t actual, int32_t kpQ16, int32_t kiQ16, int32_t feedforwardQ16,
    int32_t integralLimit, int16_t pwmLimit);

#endif /* SPEED_PI_H */
// ----- AI
