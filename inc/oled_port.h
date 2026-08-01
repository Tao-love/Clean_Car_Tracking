#ifndef __OLED_PORT_H
#define __OLED_PORT_H

/* ============================================================================
 * oled_port.h —— 硬件移植层 (MSPM0G3507, 软件模拟 I2C)
 * ----------------------------------------------------------------------------
 * 【本文件作用】整个库里唯一和单片机绑定的文件。用普通 GPIO 模拟 I2C。
 *
 * 【引脚怎么定义?  —— 用 SysConfig 配置, 不手写 PINCM (最不容易出错)】
 *   本文件不再硬写引脚编号, 而是引用 SysConfig 生成的宏。
 *   你需要在工程的 .syscfg 里添加一个名为 OLED 的 GPIO 模块, 加两个引脚:
 *      引脚名 SCL  -> 选到 PB2 (或你想要的任意脚)
 *      引脚名 SDA  -> 选到 PB3
 *   SysConfig 会自动生成下面这些宏 (在 ti_msp_dl_config.h 里):
 *      GPIO_OLED_PORT
 *      GPIO_OLED_SCL_PIN / GPIO_OLED_SCL_IOMUX
 *      GPIO_OLED_SDA_PIN / GPIO_OLED_SDA_IOMUX
 *   —— 详细步骤见 README 第三节。
 *
 * 【改引脚】直接在 SysConfig 里把 SCL/SDA 拖到别的脚即可, 本文件一个字都不用改!
 *          这就是用 SysConfig 的最大好处: 引脚编号由工具保证正确。
 *
 * 【接线】VCC->3V3(勿接5V)  GND->GND  SCL->PB2  SDA->PB3
 *
 * 【可调参数】
 *   OLED_I2C_ADDR : 从机地址。D/C#接GND=0x78; 接VCC=0x7A。
 *   OLED_I2C_DELAY: I2C 时序延时。越大越慢越稳; 花屏就调大。
 * ==========================================================================*/

#include <stdint.h>
#include "ti_msp_dl_config.h"   /* SysConfig 生成, 内含 GPIO_OLED_* 宏 */

/* ---------------- 【可改 1】I2C 从机地址 ---------------- */
#define OLED_I2C_ADDR   0x78

/* ---------------- 【可改 2】软件 I2C 时序延时 ----------- */
#define OLED_I2C_DELAY  8

/* ---------------- 引脚: 全部来自 SysConfig, 无需手改 ----- */
#define OLED_PORT          GPIO_OLED_PORT
#define OLED_SCL_PIN       GPIO_OLED_SCL_PIN
#define OLED_SCL_IOMUX     GPIO_OLED_SCL_IOMUX
#define OLED_SDA_PIN       GPIO_OLED_SDA_PIN
#define OLED_SDA_IOMUX     GPIO_OLED_SDA_IOMUX

/* ---------------- 调试观测变量 (CCS Expressions 窗口看) --- */
/* g_oled_last_ack: 最近应答 0=收到ACK(通) 1=NACK(不通)                  */
/* g_oled_ack_fail: 累计NACK次数, 一直为0=全程正常, 持续增大=接线/地址错  */
extern volatile uint8_t  g_oled_last_ack;
extern volatile uint32_t g_oled_ack_fail;

/* ---------------- 对上层的接口 ------------------------- */
void OLED_Port_Init(void);
void OLED_Port_WriteCmd(uint8_t cmd);
void OLED_Port_WriteData(uint8_t dat);
void OLED_Port_WriteDataBuf(uint8_t *buf, uint16_t len);
void OLED_Port_DelayMs(uint32_t ms);

#endif /* __OLED_PORT_H */
