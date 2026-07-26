// ----- AI
/*
 * UART2 原样回显应用入口。
 *
 * 职责：按安全顺序初始化硬件，调度非阻塞协议和 6.667 ms 任务。
 * 依赖：SysConfig、motor、UART transport、uart_echo。
 * 上下文：main 主循环；TIMG0 ISR 只递增 tick。
 * 数据单位：gControlTick 每单位 6.667 ms。
 * 硬件副作用：初始化外设、开启 PWM 计数器（比较值为 0）、UART2 和 TIMG0。
 * 故障输出：本阶段不启动电机；后续由 safety_guard 锁存故障。
 */

#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "control_params.h"
#include "key_start.h"
#include "main.h"
#include "motor.h"
#include "mpu6050.h"
#include "trial_manager.h"
#include "uart_echo.h"
#include "uart_transport.h"

#define UART_ECHO_BYTE_BUDGET (64U)

volatile uint32_t gControlTick = 0U;

static void App_StartKey1Trial(uint32_t tick);
static KeyStartState gKey1StartState;

int main(void)
{
    uint32_t lastProcessedTick = 0U;
    /* 按 SysConfig 生成配置初始化时钟、GPIO、PWM、UART 和定时器；此时 PWM 默认比较值为 0。 */
    SYSCFG_DL_init();
    MPU6050_InitAndCalibrate();
    /* Motor_Init 的第一个硬件动作是同时清零两路 PWM 和四个方向脚。 */
    Motor_Init();
    /* 手动调参版本仅使用 src/manual_tuning.c 中随固件烧录的参数表。 */
    TrialManager_Init(0U, 0, 0U);
    KeyStart_Init(&gKey1StartState);
    UART_Transport_Init();
    (void) UART_Echo_SendStartupBeacon(UART_Transport_Write);

    /* 允许 TIMG0 的 6.667 ms 周期中断进入 CPU，ISR 只递增单调 tick。 */
    NVIC_EnableIRQ(TIMER_CONTROL_INST_INT_IRQN);
    /* 启动 TIMG0 周期计数，从此每 6.667 ms 产生一个 ZERO 事件。 */
    DL_TimerG_startCounter(TIMER_CONTROL_INST);

    while (1) {
        uint32_t currentTick;

        /* 每次最多回显 64 个 UART 字节，避免噪声流长时间占用主循环。 */
        (void) UART_Echo_Poll(UART_Transport_ReadByte,
            UART_Transport_Write, UART_ECHO_BYTE_BUDGET);

        currentTick = gControlTick;
        if (currentTick != lastProcessedTick) {
            uint32_t elapsed = currentTick - lastProcessedTick;

            if (elapsed > 1U) {
                uint32_t missed = elapsed - 1U;
                TrialManager_ReportOverrun(currentTick,
                    (missed > UINT16_MAX) ? UINT16_MAX : (uint16_t) missed);
            }
            App_StartKey1Trial(currentTick);
            TrialManager_ControlTick(currentTick);
            lastProcessedTick = currentTick;
        }
    }
}

/* KEY1 为上拉输入：PA23 读到低电平表示按下。
 * 只在按下沿请求一次；TrialManager_Arm 仅允许 IDLE，因此运行中不会重启。 */
static void App_StartKey1Trial(uint32_t tick)
{
    bool key1Pressed = DL_GPIO_readPins(GPIO_KEYS_KEY1_PORT,
        GPIO_KEYS_KEY1_PIN) == 0U;

    if (!KeyStart_OnSample(&gKey1StartState, key1Pressed)) {
        return;
    }
    if (TrialManager_Arm(ControlParams_GetVersion()) != TRIAL_COMMAND_OK) {
        return;
    }
    (void) TrialManager_Start(tick, TRIAL_MODE_LINE_FOLLOW, 0, 0,
        TRIAL_START_SOURCE_KEY1);
}

void TIMER_CONTROL_INST_IRQHandler(void)
{
    /* 读取 TIMG0 最高优先级待处理中断，读取同时完成对应硬件标志的确认。 */
    switch (DL_TimerG_getPendingInterrupt(TIMER_CONTROL_INST)) {
        case DL_TIMER_IIDX_ZERO:
            gControlTick++;
            break;
        default:
            break;
    }
}
// ----- AI
