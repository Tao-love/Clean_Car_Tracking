// ----- AI
/*
 * 1-3 无线自动整定应用入口。
 *
 * 职责：按安全顺序初始化硬件，调度非阻塞协议和 10 ms 任务。
 * 依赖：SysConfig、motor、UART transport、protocol、app_protocol。
 * 上下文：main 主循环；TIMG0 ISR 只递增 tick。
 * 数据单位：gControlTick 每单位 10 ms。
 * 硬件副作用：初始化外设、开启 PWM 计数器（比较值为 0）、UART1 和 TIMG0。
 * 故障输出：本阶段不启动电机；后续由 safety_guard 锁存故障。
 */

#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "app_protocol.h"
#include "main.h"
#include "motor.h"
#include "protocol.h"
#include "uart_transport.h"

#define PROTOCOL_POLL_BYTE_BUDGET (64U)
#define INITIAL_SESSION_ID        (0xB13A0001UL)

volatile uint32_t gControlTick = 0U;

int main(void)
{
    /* 按 SysConfig 生成配置初始化时钟、GPIO、PWM、UART 和定时器；此时 PWM 默认比较值为 0。 */
    SYSCFG_DL_init();
    /* Motor_Init 的第一个硬件动作是同时清零两路 PWM 和四个方向脚。 */
    Motor_Init();
    UART_Transport_Init();
    AppProtocol_Init(INITIAL_SESSION_ID);
    Protocol_Init(AppProtocol_HandleFrame);

    /* 允许 TIMG0 的 10 ms 周期中断进入 CPU，ISR 只递增单调 tick。 */
    NVIC_EnableIRQ(TIMER_CONTROL_INST_INT_IRQN);
    /* 启动 TIMG0 周期计数，从此每 10 ms 产生一个 ZERO 事件。 */
    DL_TimerG_startCounter(TIMER_CONTROL_INST);

    while (1) {
        /* 每次最多解析 64 个 UART 字节，避免噪声流长时间占用主循环。 */
        (void) Protocol_Poll(PROTOCOL_POLL_BYTE_BUDGET);
    }
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
