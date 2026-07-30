// ----- AI
/* TB6612FNG 方向与 PWM 输出；STBY 由 PCB 固定到 5 V，不存在任何软件 STBY 操作。 */

#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "motor.h"

static int16_t gLeftPwm;
static int16_t gRightPwm;

static void Motor_SetLeftDirection(bool forward);
static void Motor_SetRightDirection(bool forward);
static uint16_t Motor_AbsSpeed(int16_t speed);

void Motor_Init(void)
{
    Motor_Stop();
    /* 启动 TIMA0 PWM 计数器；Motor_Stop 已保证两个比较值为 0，因此启动不会让电机动作。 */
    DL_TimerA_startCounter(PWM_MOTOR_INST);
}

void Motor_SetSpeed(int16_t leftPwm, int16_t rightPwm)
{
    uint16_t leftMagnitude;
    uint16_t rightMagnitude;

    leftPwm = Motor_ClampSpeed(leftPwm, MOTOR_PWM_PERIOD);
    rightPwm = Motor_ClampSpeed(rightPwm, MOTOR_PWM_PERIOD);
    Motor_SetLeftDirection(leftPwm >= 0);
    Motor_SetRightDirection(rightPwm >= 0);
    leftMagnitude = Motor_AbsSpeed(leftPwm);
    rightMagnitude = Motor_AbsSpeed(rightPwm);

    /* 将左轮 PWM 幅值写入 TIMA0 CCP0 比较寄存器，数值为 0..1600。 */
    DL_TimerA_setCaptureCompareValue(
        PWM_MOTOR_INST, leftMagnitude, GPIO_PWM_MOTOR_C0_IDX);
    /* 将右轮 PWM 幅值写入 TIMA0 CCP2 比较寄存器，数值为 0..1600。 */
    DL_TimerA_setCaptureCompareValue(
        PWM_MOTOR_INST, rightMagnitude, GPIO_PWM_MOTOR_C2_IDX);
    gLeftPwm = leftPwm;
    gRightPwm = rightPwm;
}

void Motor_Stop(void)
{
    /* 立即把左轮 TIMA0 CCP0 比较值清零，使 PWMA 无有效占空比。 */
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0U, GPIO_PWM_MOTOR_C0_IDX);
    /* 立即把右轮 TIMA0 CCP2 比较值清零，使 PWMB 无有效占空比。 */
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0U, GPIO_PWM_MOTOR_C2_IDX);
    /* 同时拉低 AIN1/AIN2/BIN1/BIN2，避免 STBY 固定高时任一方向组合残留。 */
    DL_GPIO_clearPins(GPIO_MOTOR_PORT,
        GPIO_MOTOR_MOTOR_AIN1_PIN | GPIO_MOTOR_MOTOR_AIN2_PIN |
        GPIO_MOTOR_MOTOR_BIN1_PIN | GPIO_MOTOR_MOTOR_BIN2_PIN);
    gLeftPwm = 0;
    gRightPwm = 0;
}

int16_t Motor_ClampSpeed(int16_t speed, int16_t maxAbsSpeed)
{
    if (maxAbsSpeed < 0) {
        maxAbsSpeed = (int16_t) -maxAbsSpeed;
    }
    if (speed > maxAbsSpeed) {
        return maxAbsSpeed;
    }
    if (speed < -maxAbsSpeed) {
        return (int16_t) -maxAbsSpeed;
    }
    return speed;
}

int16_t Motor_GetLeftPwm(void)
{
    return gLeftPwm;
}

int16_t Motor_GetRightPwm(void)
{
    return gRightPwm;
}

static void Motor_SetLeftDirection(bool forward)
{
    if (forward) {
        /* 左轮正转：拉高 AIN1。 */
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_AIN1_PIN);
        /* 左轮正转：同时拉低 AIN2，形成唯一方向组合。 */
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_AIN2_PIN);
    } else {
        /* 左轮反转：拉低 AIN1。 */
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_AIN1_PIN);
        /* 左轮反转：同时拉高 AIN2。 */
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_AIN2_PIN);
    }
}

static void Motor_SetRightDirection(bool forward)
{
    if (forward) {
        /* 右轮车前进：BIN1 高、BIN2 低。 */
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN2_PIN);
    } else {
        /* 右轮反转：BIN1 低、BIN2 高。 */
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN1_PIN);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN2_PIN);
    }
}

static uint16_t Motor_AbsSpeed(int16_t speed)
{
    return (speed < 0) ? (uint16_t) -speed : (uint16_t) speed;
}
// ----- AI
