/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
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

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)



#define CPUCLK_FREQ                                                     32000000



/* Defines for PWM_MOTOR */
#define PWM_MOTOR_INST                                                     TIMA0
#define PWM_MOTOR_INST_IRQHandler                               TIMA0_IRQHandler
#define PWM_MOTOR_INST_INT_IRQN                                 (TIMA0_INT_IRQn)
#define PWM_MOTOR_INST_CLK_FREQ                                         32000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_MOTOR_C0_PORT                                             GPIOB
#define GPIO_PWM_MOTOR_C0_PIN                                     DL_GPIO_PIN_14
#define GPIO_PWM_MOTOR_C0_IOMUX                                  (IOMUX_PINCM31)
#define GPIO_PWM_MOTOR_C0_IOMUX_FUNC                 IOMUX_PINCM31_PF_TIMA0_CCP0
#define GPIO_PWM_MOTOR_C0_IDX                                DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 2 */
#define GPIO_PWM_MOTOR_C2_PORT                                             GPIOA
#define GPIO_PWM_MOTOR_C2_PIN                                      DL_GPIO_PIN_7
#define GPIO_PWM_MOTOR_C2_IOMUX                                  (IOMUX_PINCM14)
#define GPIO_PWM_MOTOR_C2_IOMUX_FUNC                 IOMUX_PINCM14_PF_TIMA0_CCP2
#define GPIO_PWM_MOTOR_C2_IDX                                DL_TIMER_CC_2_INDEX



/* Defines for TIMER_CONTROL */
#define TIMER_CONTROL_INST                                               (TIMG0)
#define TIMER_CONTROL_INST_IRQHandler                           TIMG0_IRQHandler
#define TIMER_CONTROL_INST_INT_IRQN                             (TIMG0_INT_IRQn)
#define TIMER_CONTROL_INST_LOAD_VALUE                                     (217U)




/* Defines for I2C_MPU6050 */
#define I2C_MPU6050_INST                                                    I2C1
#define I2C_MPU6050_INST_IRQHandler                              I2C1_IRQHandler
#define I2C_MPU6050_INST_INT_IRQN                                  I2C1_INT_IRQn
#define I2C_MPU6050_BUS_SPEED_HZ                                          400000
#define GPIO_I2C_MPU6050_SDA_PORT                                          GPIOB
#define GPIO_I2C_MPU6050_SDA_PIN                                   DL_GPIO_PIN_3
#define GPIO_I2C_MPU6050_IOMUX_SDA                               (IOMUX_PINCM16)
#define GPIO_I2C_MPU6050_IOMUX_SDA_FUNC                IOMUX_PINCM16_PF_I2C1_SDA
#define GPIO_I2C_MPU6050_SCL_PORT                                          GPIOB
#define GPIO_I2C_MPU6050_SCL_PIN                                   DL_GPIO_PIN_2
#define GPIO_I2C_MPU6050_IOMUX_SCL                               (IOMUX_PINCM15)
#define GPIO_I2C_MPU6050_IOMUX_SCL_FUNC                IOMUX_PINCM15_PF_I2C1_SCL


/* Defines for UART_DEBUG */
#define UART_DEBUG_INST                                                    UART2
#define UART_DEBUG_INST_FREQUENCY                                       32000000
#define UART_DEBUG_INST_IRQHandler                              UART2_IRQHandler
#define UART_DEBUG_INST_INT_IRQN                                  UART2_INT_IRQn
#define GPIO_UART_DEBUG_RX_PORT                                            GPIOB
#define GPIO_UART_DEBUG_TX_PORT                                            GPIOB
#define GPIO_UART_DEBUG_RX_PIN                                    DL_GPIO_PIN_16
#define GPIO_UART_DEBUG_TX_PIN                                    DL_GPIO_PIN_15
#define GPIO_UART_DEBUG_IOMUX_RX                                 (IOMUX_PINCM33)
#define GPIO_UART_DEBUG_IOMUX_TX                                 (IOMUX_PINCM32)
#define GPIO_UART_DEBUG_IOMUX_RX_FUNC                  IOMUX_PINCM33_PF_UART2_RX
#define GPIO_UART_DEBUG_IOMUX_TX_FUNC                  IOMUX_PINCM32_PF_UART2_TX
#define UART_DEBUG_BAUD_RATE                                            (115200)
#define UART_DEBUG_IBRD_32_MHZ_115200_BAUD                                  (17)
#define UART_DEBUG_FBRD_32_MHZ_115200_BAUD                                  (23)





/* Port definition for Pin Group GPIO_MOTOR */
#define GPIO_MOTOR_PORT                                                  (GPIOB)

/* Defines for MOTOR_AIN1: GPIOB.9 with pinCMx 26 on package pin 61 */
#define GPIO_MOTOR_MOTOR_AIN1_PIN                                (DL_GPIO_PIN_9)
#define GPIO_MOTOR_MOTOR_AIN1_IOMUX                              (IOMUX_PINCM26)
/* Defines for MOTOR_AIN2: GPIOB.10 with pinCMx 27 on package pin 62 */
#define GPIO_MOTOR_MOTOR_AIN2_PIN                               (DL_GPIO_PIN_10)
#define GPIO_MOTOR_MOTOR_AIN2_IOMUX                              (IOMUX_PINCM27)
/* Defines for MOTOR_BIN1: GPIOB.7 with pinCMx 24 on package pin 59 */
#define GPIO_MOTOR_MOTOR_BIN1_PIN                                (DL_GPIO_PIN_7)
#define GPIO_MOTOR_MOTOR_BIN1_IOMUX                              (IOMUX_PINCM24)
/* Defines for MOTOR_BIN2: GPIOB.6 with pinCMx 23 on package pin 58 */
#define GPIO_MOTOR_MOTOR_BIN2_PIN                                (DL_GPIO_PIN_6)
#define GPIO_MOTOR_MOTOR_BIN2_IOMUX                              (IOMUX_PINCM23)
/* Defines for LINE_D0: GPIOB.19 with pinCMx 45 on package pin 16 */
#define GPIO_LINE_LINE_D0_PORT                                           (GPIOB)
#define GPIO_LINE_LINE_D0_PIN                                   (DL_GPIO_PIN_19)
#define GPIO_LINE_LINE_D0_IOMUX                                  (IOMUX_PINCM45)
/* Defines for LINE_D1: GPIOB.17 with pinCMx 43 on package pin 14 */
#define GPIO_LINE_LINE_D1_PORT                                           (GPIOB)
#define GPIO_LINE_LINE_D1_PIN                                   (DL_GPIO_PIN_17)
#define GPIO_LINE_LINE_D1_IOMUX                                  (IOMUX_PINCM43)
/* Defines for LINE_D2: GPIOA.16 with pinCMx 38 on package pin 9 */
#define GPIO_LINE_LINE_D2_PORT                                           (GPIOA)
#define GPIO_LINE_LINE_D2_PIN                                   (DL_GPIO_PIN_16)
#define GPIO_LINE_LINE_D2_IOMUX                                  (IOMUX_PINCM38)
/* Defines for LINE_D3: GPIOA.14 with pinCMx 36 on package pin 7 */
#define GPIO_LINE_LINE_D3_PORT                                           (GPIOA)
#define GPIO_LINE_LINE_D3_PIN                                   (DL_GPIO_PIN_14)
#define GPIO_LINE_LINE_D3_IOMUX                                  (IOMUX_PINCM36)
/* Defines for LINE_D4: GPIOB.20 with pinCMx 48 on package pin 19 */
#define GPIO_LINE_LINE_D4_PORT                                           (GPIOB)
#define GPIO_LINE_LINE_D4_PIN                                   (DL_GPIO_PIN_20)
#define GPIO_LINE_LINE_D4_IOMUX                                  (IOMUX_PINCM48)
/* Defines for LINE_D5: GPIOB.25 with pinCMx 56 on package pin 27 */
#define GPIO_LINE_LINE_D5_PORT                                           (GPIOB)
#define GPIO_LINE_LINE_D5_PIN                                   (DL_GPIO_PIN_25)
#define GPIO_LINE_LINE_D5_IOMUX                                  (IOMUX_PINCM56)
/* Defines for LINE_D6: GPIOA.25 with pinCMx 55 on package pin 26 */
#define GPIO_LINE_LINE_D6_PORT                                           (GPIOA)
#define GPIO_LINE_LINE_D6_PIN                                   (DL_GPIO_PIN_25)
#define GPIO_LINE_LINE_D6_IOMUX                                  (IOMUX_PINCM55)
/* Defines for LINE_D7: GPIOA.27 with pinCMx 60 on package pin 31 */
#define GPIO_LINE_LINE_D7_PORT                                           (GPIOA)
#define GPIO_LINE_LINE_D7_PIN                                   (DL_GPIO_PIN_27)
#define GPIO_LINE_LINE_D7_IOMUX                                  (IOMUX_PINCM60)
/* Port definition for Pin Group GPIO_ENCODER */
#define GPIO_ENCODER_PORT                                                (GPIOB)

/* Defines for ENCODER_LEFT_A: GPIOB.11 with pinCMx 28 on package pin 63 */
// pins affected by this interrupt request:["ENCODER_LEFT_A","ENCODER_RIGHT_A"]
#define GPIO_ENCODER_INT_IRQN                                   (GPIOB_INT_IRQn)
#define GPIO_ENCODER_INT_IIDX                   (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define GPIO_ENCODER_ENCODER_LEFT_A_IIDX                    (DL_GPIO_IIDX_DIO11)
#define GPIO_ENCODER_ENCODER_LEFT_A_PIN                         (DL_GPIO_PIN_11)
#define GPIO_ENCODER_ENCODER_LEFT_A_IOMUX                        (IOMUX_PINCM28)
/* Defines for ENCODER_LEFT_B: GPIOB.12 with pinCMx 29 on package pin 64 */
#define GPIO_ENCODER_ENCODER_LEFT_B_PIN                         (DL_GPIO_PIN_12)
#define GPIO_ENCODER_ENCODER_LEFT_B_IOMUX                        (IOMUX_PINCM29)
/* Defines for ENCODER_RIGHT_A: GPIOB.4 with pinCMx 17 on package pin 52 */
#define GPIO_ENCODER_ENCODER_RIGHT_A_IIDX                    (DL_GPIO_IIDX_DIO4)
#define GPIO_ENCODER_ENCODER_RIGHT_A_PIN                         (DL_GPIO_PIN_4)
#define GPIO_ENCODER_ENCODER_RIGHT_A_IOMUX                       (IOMUX_PINCM17)
/* Defines for ENCODER_RIGHT_B: GPIOB.5 with pinCMx 18 on package pin 53 */
#define GPIO_ENCODER_ENCODER_RIGHT_B_PIN                         (DL_GPIO_PIN_5)
#define GPIO_ENCODER_ENCODER_RIGHT_B_IOMUX                       (IOMUX_PINCM18)
/* Defines for BUZZER: GPIOB.27 with pinCMx 58 on package pin 29 */
#define GPIO_UI_BUZZER_PORT                                              (GPIOB)
#define GPIO_UI_BUZZER_PIN                                      (DL_GPIO_PIN_27)
#define GPIO_UI_BUZZER_IOMUX                                     (IOMUX_PINCM58)
/* Defines for ULTRASONIC_TRIG: GPIOA.10 with pinCMx 21 on package pin 56 */
#define GPIO_UI_ULTRASONIC_TRIG_PORT                                     (GPIOA)
#define GPIO_UI_ULTRASONIC_TRIG_PIN                             (DL_GPIO_PIN_10)
#define GPIO_UI_ULTRASONIC_TRIG_IOMUX                            (IOMUX_PINCM21)
/* Defines for ULTRASONIC_ECHO: GPIOA.11 with pinCMx 22 on package pin 57 */
#define GPIO_UI_ULTRASONIC_ECHO_PORT                                     (GPIOA)
#define GPIO_UI_ULTRASONIC_ECHO_PIN                             (DL_GPIO_PIN_11)
#define GPIO_UI_ULTRASONIC_ECHO_IOMUX                            (IOMUX_PINCM22)
/* Defines for KEY1: GPIOA.23 with pinCMx 53 on package pin 24 */
#define GPIO_KEYS_KEY1_PORT                                              (GPIOA)
#define GPIO_KEYS_KEY1_PIN                                      (DL_GPIO_PIN_23)
#define GPIO_KEYS_KEY1_IOMUX                                     (IOMUX_PINCM53)
/* Defines for KEY2: GPIOA.21 with pinCMx 46 on package pin 17 */
#define GPIO_KEYS_KEY2_PORT                                              (GPIOA)
#define GPIO_KEYS_KEY2_PIN                                      (DL_GPIO_PIN_21)
#define GPIO_KEYS_KEY2_IOMUX                                     (IOMUX_PINCM46)
/* Defines for KEY3: GPIOB.18 with pinCMx 44 on package pin 15 */
#define GPIO_KEYS_KEY3_PORT                                              (GPIOB)
#define GPIO_KEYS_KEY3_PIN                                      (DL_GPIO_PIN_18)
#define GPIO_KEYS_KEY3_IOMUX                                     (IOMUX_PINCM44)
/* Defines for KEY4: GPIOA.17 with pinCMx 39 on package pin 10 */
#define GPIO_KEYS_KEY4_PORT                                              (GPIOA)
#define GPIO_KEYS_KEY4_PIN                                      (DL_GPIO_PIN_17)
#define GPIO_KEYS_KEY4_IOMUX                                     (IOMUX_PINCM39)
/* Port definition for Pin Group GPIO_LED */
#define GPIO_LED_PORT                                                    (GPIOA)

/* Defines for LED1: GPIOA.15 with pinCMx 37 on package pin 8 */
#define GPIO_LED_LED1_PIN                                       (DL_GPIO_PIN_15)
#define GPIO_LED_LED1_IOMUX                                      (IOMUX_PINCM37)
/* Defines for LED2: GPIOA.22 with pinCMx 47 on package pin 18 */
#define GPIO_LED_LED2_PIN                                       (DL_GPIO_PIN_22)
#define GPIO_LED_LED2_IOMUX                                      (IOMUX_PINCM47)
/* Port definition for Pin Group GPIO_OLED */
#define GPIO_OLED_PORT                                                   (GPIOA)

/* Defines for SCL: GPIOA.1 with pinCMx 2 on package pin 34 */
#define GPIO_OLED_SCL_PIN                                        (DL_GPIO_PIN_1)
#define GPIO_OLED_SCL_IOMUX                                       (IOMUX_PINCM2)
/* Defines for SDA: GPIOA.0 with pinCMx 1 on package pin 33 */
#define GPIO_OLED_SDA_PIN                                        (DL_GPIO_PIN_0)
#define GPIO_OLED_SDA_IOMUX                                       (IOMUX_PINCM1)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_PWM_MOTOR_init(void);
void SYSCFG_DL_TIMER_CONTROL_init(void);
void SYSCFG_DL_I2C_MPU6050_init(void);
void SYSCFG_DL_UART_DEBUG_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
