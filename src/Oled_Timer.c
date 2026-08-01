/* Oled_Timer.c —— OLED 计时显示模块实现 (TIMG6 1 秒定时 + SSD1306 OLED) */

#include "Oled_Timer.h"
#include "oled.h"
#include "ti_msp_dl_config.h"

/* ---- ISR 与主循环共享状态（volatile）---- */
static volatile uint32_t gOledSeconds;      /* 从本次启动起流逝的秒数 */
static volatile bool     gOledTimingActive; /* true = ISR 累加秒数, false = 冻结 */
static volatile bool     gOledDisplayDirty; /* ISR 置 true，主循环刷新后清 false */

/* ---- 仅主循环访问 ---- */
static uint32_t gOledLastDisplayed;         /* 上次已渲染到 OLED 的秒数值 */

/* ============================================================================
 * TIMG6_IRQHandler —— 1 秒周期中断（覆盖 startup 文件中的 weak 别名）
 *
 * 与 TIMG0 ISR 模式一致：读取最高优先级待处理中断，switch 分发。
 * 仅在 gOledTimingActive 为 true 时累加秒数并置脏标记。
 * ==========================================================================*/
void TIMG6_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(TIMG6)) {
        case DL_TIMER_IIDX_ZERO:
            if (gOledTimingActive) {
                gOledSeconds++;
                gOledDisplayDirty = true;
            }
            break;
        default:
            break;
    }
}

/* ============================================================================
 * OledTimer_Init —— 初始化 TIMG6 和 OLED
 *
 * 流程：TIMG6 上电复位 → 配置时钟 → 配置定时器模式 → 使能中断 → 使能时钟
 *       → NVIC 使能 → OLED 初始化 → 绘制初始 "Time: 00 s"
 *
 * TIMG6 配置：LFCLK 32768 Hz, /1 预分频, period=32767 → 1 秒 ZERO 事件
 * 公式: (1.0 sec * 32768 Hz) - 1 = 32767
 * ==========================================================================*/
void OledTimer_Init(void)
{
    /* 1. 内部状态清零 */
    gOledSeconds       = 0U;
    gOledTimingActive  = false;
    gOledDisplayDirty  = false;
    gOledLastDisplayed = 0U;

    /* 2. TIMG6 上电与复位（TIMG6 不在 SysConfig 中，需手动操作） */
    DL_TimerG_reset(TIMG6);
    DL_TimerG_enablePower(TIMG6);

    /* 3. 时钟配置：LFCLK 32768 Hz, /1 预分频, /1 分频比
       结果: timerClkFreq = 32768 / (1 * (0 + 1)) = 32768 Hz          */
    {
        DL_TimerG_ClockConfig clockCfg = {
            .clockSel    = DL_TIMER_CLOCK_LFCLK,
            .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
            .prescale    = 0U,
        };
        DL_TimerG_setClockConfig(TIMG6, &clockCfg);
    }

    /* 4. 定时器模式：周期性减计数，period=32767 → 1 Hz, 暂不启动 */
    {
        DL_TimerG_TimerConfig timerCfg = {
            .period     = 32767U,
            .timerMode  = DL_TIMER_TIMER_MODE_PERIODIC,
            .startTimer = DL_TIMER_STOP,
        };
        DL_TimerG_initTimerMode(TIMG6, &timerCfg);
    }

    /* 5. 使能 ZERO 事件中断 */
    DL_TimerG_enableInterrupt(TIMG6, DL_TIMERG_INTERRUPT_ZERO_EVENT);

    /* 6. 使能定时器时钟 */
    DL_TimerG_enableClock(TIMG6);

    /* 7. NVIC 使能 TIMG6 中断线 */
    NVIC_EnableIRQ(TIMG6_INT_IRQn);

    /* 8. 初始化 OLED 硬件（含 100ms 上电等待、命令序列、清屏、刷新） */
    OLED_Init();

    /* 9. 绘制初始显示: "Time: 00 s"（8x16 字体, 左对齐）
          "Time: " = cols 0~47, "00" = cols 48~63, " s" = cols 64~79   */
    OLED_ShowString(0, 0, "Time: ", 16);
    OLED_ShowNum(48, 0, 0, 2, 16);
    OLED_ShowString(64, 0, " s", 16);
    OLED_Refresh();
}

/* ============================================================================
 * OledTimer_StartTiming —— 清零并开始计时
 *
 * 每次运动开始时调用。秒数归零、打开门控、置脏标记、启动硬件计数器。
 * ==========================================================================*/
void OledTimer_StartTiming(void)
{
    /* 每轮都从完整 1 秒周期开始，不能沿用上一轮停止时的 TIMG6 相位。 */
    DL_TimerG_stopCounter(TIMG6);
    DL_TimerG_setTimerCount(TIMG6, 32767U);
    DL_TimerG_clearInterruptStatus(TIMG6, DL_TIMERG_INTERRUPT_ZERO_EVENT);
    gOledSeconds       = 0U;
    gOledTimingActive  = true;
    gOledDisplayDirty  = true;
    gOledLastDisplayed = UINT32_MAX; /* 强制主循环重绘本轮首帧 "00"。 */
    DL_TimerG_startCounter(TIMG6);
}

/* ============================================================================
 * OledTimer_StopTiming —— 冻结计时
 *
 * 关闭软件门控并让硬件计数器回到完整周期起点，下一次启动不会提前跳秒。
 * ==========================================================================*/
void OledTimer_StopTiming(void)
{
    gOledTimingActive = false;
    DL_TimerG_stopCounter(TIMG6);
    DL_TimerG_setTimerCount(TIMG6, 32767U);
    DL_TimerG_clearInterruptStatus(TIMG6, DL_TIMERG_INTERRUPT_ZERO_EVENT);
}

/* ============================================================================
 * OledTimer_UpdateDisplay —— 主循环轮询刷新 OLED
 *
 * 设计为无条件轮询，内部双重快速退出：
 *   1. gOledDisplayDirty 为 false → 立即返回（最常见，几乎零开销）
 *   2. currentSeconds == gOledLastDisplayed → 返回（防抖/边缘情况）
 *
 * 仅重绘数字部分（"XX"），静态文字 "Time: " 和 " s" 已在 Init 时写入
 * 帧缓冲且永不擦除，无需重复绘制。
 * ==========================================================================*/
void OledTimer_UpdateDisplay(void)
{
    uint32_t currentSeconds;

    if (!gOledDisplayDirty) {
        return;
    }

    currentSeconds = gOledSeconds;  /* 32-bit 原子读，无需临界区 */

    if (currentSeconds == gOledLastDisplayed) {
        return;
    }

    /* 仅更新数字部分：x=48, y=0, 2 位数字, 8x16 字体 */
    OLED_ShowNum(48, 0, currentSeconds, 2, 16);
    OLED_Refresh();

    gOledLastDisplayed = currentSeconds;
    gOledDisplayDirty  = false;
}

/* ============================================================================
 * OledTimer_GetSeconds —— 查询当前秒数
 * ==========================================================================*/
uint32_t OledTimer_GetSeconds(void)
{
    return gOledSeconds;
}
