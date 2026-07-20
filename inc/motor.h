// ----- AI
/*
 * TB6612FNG 双电机输出模块。
 * 职责：设置左右方向与 PWM，并提供全系统唯一的硬件停车底层动作。
 * 依赖：SysConfig GPIO_MOTOR/PWM_MOTOR。上下文：主循环，禁止 ISR 调用。
 * 单位：有符号 PWM 比较值，范围 -1600..1600。
 * 硬件副作用：直接改变 PWMA/PWMB 和 AIN1/AIN2/BIN1/BIN2。STBY 固定 5 V，本模块无法控制。
 * 故障输出：无；上层必须记录停止原因。
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

#define MOTOR_PWM_PERIOD (1600)

/* 用途：先统一停车，再启动 PWM 计数器；禁止 ISR 调用。 */
void Motor_Init(void);

/* 用途：设置左右有符号 PWM，内部硬限幅到 1600；会改变电机引脚，禁止 ISR 调用。 */
void Motor_SetSpeed(int16_t leftPwm, int16_t rightPwm);

/*
 * 用途：同一路径立即将两路 PWM 设 0，并将四个方向脚全部拉低。
 * 输入输出：无。副作用：电机软件停车，但 STBY 仍为 5 V；禁止 ISR 调用。
 */
void Motor_Stop(void);

/* 用途：限制有符号 PWM 绝对值；无硬件副作用。 */
int16_t Motor_ClampSpeed(int16_t speed, int16_t maxAbsSpeed);

/* 用途：返回最近一次写入的左/右 PWM，供安全和统计使用。 */
int16_t Motor_GetLeftPwm(void);
int16_t Motor_GetRightPwm(void);

#endif /* MOTOR_H */
// ----- AI
