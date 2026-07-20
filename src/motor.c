#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "motor.h"

static void Motor_SetLeftDirection(bool forward);
static void Motor_SetRightDirection(bool forward);
static uint16_t Motor_AbsSpeed(int16_t speed);

void Motor_Init(void)
{
    Motor_Stop();
    DL_TimerA_startCounter(PWM_MOTOR_INST);
}

void Motor_SetSpeed(int16_t leftSpeed, int16_t rightSpeed)
{
    uint16_t leftPwm;
    uint16_t rightPwm;

    Motor_SetLeftDirection(leftSpeed >= 0);
    Motor_SetRightDirection(rightSpeed >= 0);

    leftPwm = Motor_AbsSpeed(leftSpeed);
    rightPwm = Motor_AbsSpeed(rightSpeed);

    if (leftPwm > MOTOR_PWM_PERIOD) {
        leftPwm = MOTOR_PWM_PERIOD;
    }
    if (rightPwm > MOTOR_PWM_PERIOD) {
        rightPwm = MOTOR_PWM_PERIOD;
    }

    DL_TimerA_setCaptureCompareValue(
        PWM_MOTOR_INST, leftPwm, GPIO_PWM_MOTOR_C0_IDX);
    DL_TimerA_setCaptureCompareValue(
        PWM_MOTOR_INST, rightPwm, GPIO_PWM_MOTOR_C2_IDX);
}

void Motor_Stop(void)
{
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0, GPIO_PWM_MOTOR_C0_IDX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0, GPIO_PWM_MOTOR_C2_IDX);

    DL_GPIO_clearPins(GPIO_MOTOR_PORT,
        GPIO_MOTOR_MOTOR_AIN1_PIN | GPIO_MOTOR_MOTOR_AIN2_PIN |
        GPIO_MOTOR_MOTOR_BIN1_PIN | GPIO_MOTOR_MOTOR_BIN2_PIN);
}

int16_t Motor_ClampSpeed(int16_t speed, int16_t maxAbsSpeed)
{
    if (speed > maxAbsSpeed) {
        return maxAbsSpeed;
    }
    if (speed < -maxAbsSpeed) {
        return -maxAbsSpeed;
    }
    return speed;
}

static void Motor_SetLeftDirection(bool forward)
{
    if (forward) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_AIN1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_AIN2_PIN);
    } else {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_AIN1_PIN);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_AIN2_PIN);
    }
}

static void Motor_SetRightDirection(bool forward)
{
    if (forward) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN2_PIN);
    } else {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN1_PIN);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN2_PIN);
    }
}

static uint16_t Motor_AbsSpeed(int16_t speed)
{
    if (speed < 0) {
        return (uint16_t) (-speed);
    }
    return (uint16_t) speed;
}
