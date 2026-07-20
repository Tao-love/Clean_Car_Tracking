#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

#define MOTOR_PWM_PERIOD (1600)

void Motor_Init(void);
void Motor_SetSpeed(int16_t leftSpeed, int16_t rightSpeed);
void Motor_Stop(void);
int16_t Motor_ClampSpeed(int16_t speed, int16_t maxAbsSpeed);

#endif /* MOTOR_H */
