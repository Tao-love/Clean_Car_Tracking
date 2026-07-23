// ----- AI
/*
 * UART 原样回显轮询。
 *
 * 职责：有界读取字节并逐字节提交原样发送；不访问寄存器。
 * 依赖：由调用者提供的读写回调。
 * 上下文：主循环调用，不允许 ISR 调用。
 */

#ifndef UART_ECHO_H
#define UART_ECHO_H

#include <stdbool.h>
#include <stdint.h>

typedef bool (*UART_EchoReadByte)(uint8_t *value);
typedef bool (*UART_EchoWriteBytes)(
    const uint8_t *data, uint16_t length, bool highPriority);

/* 用途：最多回显 byteBudget 个字节；返回实际消费的 RX 字节数。 */
uint16_t UART_Echo_Poll(UART_EchoReadByte readByte,
    UART_EchoWriteBytes writeBytes, uint16_t byteBudget);

/* 用途：提交一次 UART2 启动提示；返回 true 表示完整提示已入队。 */
bool UART_Echo_SendStartupBeacon(UART_EchoWriteBytes writeBytes);

#endif /* UART_ECHO_H */
// ----- AI
