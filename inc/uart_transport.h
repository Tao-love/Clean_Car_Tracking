// ----- AI
/*
 * UART2 非阻塞字节搬运模块。
 *
 * 职责：在 PCB USART2 的 UART2 硬件 FIFO 与内存环形队列之间搬运字节。
 * 依赖：SysConfig 生成的 UART_DEBUG，ring_buffer。
 * 上下文：ISR 只执行 FIFO/队列搬运；主循环读 RX、写 TX。
 * 数据单位：字节。硬件副作用：开启 UART2 NVIC 和 TX/RX 中断。
 * 故障输出：记录 RX 溢出、高/低优先级 TX 丢弃数，从不阻塞 100 Hz 控制。
 */

#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t rxOverflowBytes;
    uint32_t txHighRejectedFrames;
    uint32_t txLowRejectedFrames;
} UARTTransportCounters;

/* 用途：清空队列并开启 UART2 RX 中断；无输入输出；不允许 ISR 调用。 */
void UART_Transport_Init(void);

/*
 * 用途：从 RX 队列取一字节。
 * 输出：true=成功，false=当前无数据；成功时写入 value。
 * 副作用：消费 RX 队列。ISR 可用性：不允许。
 */
bool UART_Transport_ReadByte(uint8_t *value);

/*
 * 用途：把一整帧原子加入 TX 队列，highPriority 用于 ACK/汇总。
 * 输入：data、length（字节）、优先级。输出：true=整帧入队，false=空间不足。
 * 前置条件：length>0 时 data 有效。副作用：可开启 UART TX 中断。ISR 不可调用。
 */
bool UART_Transport_Write(
    const uint8_t *data, uint16_t length, bool highPriority);

/* 用途：读取链路丢弃计数；无副作用，ISR 不可调用。 */
UARTTransportCounters UART_Transport_GetCounters(void);

#endif /* UART_TRANSPORT_H */
// ----- AI
