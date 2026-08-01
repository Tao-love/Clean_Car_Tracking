#ifndef OLED_TIMER_H
#define OLED_TIMER_H

/* ============================================================================
 * Oled_Timer.h —— OLED 计时显示模块对外接口
 * ----------------------------------------------------------------------------
 * 【功能】在 OLED 上显示 "Time: XX s" 格式的计时器。
 *         运动开始时从 0 开始计时，运动停止时冻结显示。
 *         上电后立即显示 "Time: 00 s"。
 *
 * 【依赖】oled.h / oled_port.h（OLED 驱动），TIMG6 硬件定时器。
 *
 * 【用法】
 *   1. main() 中 SYSCFG_DL_init() 之后调用 OledTimer_Init()。
 *   2. 运动开始时调用 OledTimer_StartTiming()（清零并开始）。
 *   3. 运动停止时调用 OledTimer_StopTiming()（冻结显示）。
 *   4. 主循环中调用 OledTimer_UpdateDisplay()（无条件轮询，内部快速退出）。
 * ==========================================================================*/

#include <stdint.h>
#include <stdbool.h>

/* 初始化 TIMG6 硬件和 OLED，显示 "Time: 00 s"。
   必须在 SYSCFG_DL_init() 之后、主循环之前调用一次。 */
void OledTimer_Init(void);

/* 清零秒数、打开计时门控、启动 TIMG6 计数器。
   在 LineRun_StartProfile() 中 gRunning=true 之后调用。 */
void OledTimer_StartTiming(void);

/* 关闭计时门控，冻结当前秒数显示。
   在 LineRun_Stop() 中 Motor_Stop() 之后调用。 */
void OledTimer_StopTiming(void);

/* 主循环轮询：仅当秒数变化时才刷新 OLED 显示。
   内部有快速退出路径，不阻塞主循环。 */
void OledTimer_UpdateDisplay(void);

/* 查询当前流逝秒数（调试/遥测用）。 */
uint32_t OledTimer_GetSeconds(void);

#endif /* OLED_TIMER_H */
