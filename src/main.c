/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdbool.h>

#include "ti_msp_dl_config.h"
#include "main.h"
#include "motor.h"
#include "adc.h"
#include "pid.h"
#include "mpu6050.h"
#include "encoder.h"

#define CAR_BASE_SPEED      (350)
#define CAR_MAX_SPEED       (800)
#define CAR_CONTROL_PERIOD  (1)

volatile bool gControlFlag = false;
volatile int16_t gGyroZ = 0;
volatile int16_t gLeftEncoderSpeed = 0;
volatile int16_t gRightEncoderSpeed = 0;
volatile int16_t gLeftEncoderRawSpeed = 0;
volatile int16_t gRightEncoderRawSpeed = 0;

int main(void)
{
    SYSCFG_DL_init();
    Encoder_Init();
    Motor_Init();
    PID_Init();
    if (MPU6050_Init()) {
        (void) MPU6050_CalibrateGyroZ(MPU6050_CALIBRATION_SAMPLES);
    }

    NVIC_EnableIRQ(TIMER_CONTROL_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_CONTROL_INST);

    while (1) {
        if (gControlFlag == false) {
            continue;
        }

        gControlFlag = false;

        int16_t error = LineSensor_GetError();
        int16_t correction = PID_Calculate(error);
        int16_t gyroZ = 0;

        if (MPU6050_ReadGyroZ(&gyroZ)) {
            gGyroZ = gyroZ;
        }

        Encoder_UpdateSpeeds();
        gLeftEncoderSpeed = Encoder_GetLeftSpeed();
        gRightEncoderSpeed = Encoder_GetRightSpeed();
        gLeftEncoderRawSpeed = Encoder_GetLeftRawSpeed();
        gRightEncoderRawSpeed = Encoder_GetRightRawSpeed();

        int16_t leftSpeed = CAR_BASE_SPEED - correction;
        int16_t rightSpeed = CAR_BASE_SPEED + correction;

        Motor_SetSpeed(Motor_ClampSpeed(leftSpeed, CAR_MAX_SPEED),
            Motor_ClampSpeed(rightSpeed, CAR_MAX_SPEED));
    }
}

void TIMER_CONTROL_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(TIMER_CONTROL_INST)) {
        case DL_TIMER_IIDX_ZERO:
            gControlFlag = true;
            break;
        default:
            break;
    }
}
