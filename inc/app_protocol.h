// ----- AI
/*
 * 协议业务分发层。
 * 职责：把通过 CRC 的命令映射到状态机/参数模块，并统一生成 ACK/NACK。
 * 首个实施阶段只开放 HELLO，其他命令明确 NACK，电机不会因通信启动。
 * 上下文：主循环。硬件副作用：仅发 UART 帧。故障输出：NACK 原因码。
 */

#ifndef APP_PROTOCOL_H
#define APP_PROTOCOL_H

#include <stdint.h>

#include "protocol.h"

/* 用途：设置本次上电会话 ID 并清空业务状态；不允许 ISR 调用。 */
void AppProtocol_Init(uint32_t sessionId);

/* 用途：处理一个已通过协议校验的帧；输出 ACK/NACK；不允许 ISR 调用。 */
void AppProtocol_HandleFrame(const ProtocolFrame *frame);

#endif /* APP_PROTOCOL_H */
// ----- AI
