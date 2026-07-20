// ----- AI
/*
 * JDY-31 二进制帧协议模块。
 *
 * 职责：从 UART 字节流重同步并校验帧，将完整命令交给业务层，编码回传帧。
 * 依赖：crc16、uart_transport。上下文：只在主循环 poll/发送，禁止 ISR 调用。
 * 字节序：小端。硬件副作用：间接向 UART TX 队列入队。
 * 故障输出：记录噪声、长度、CRC 和版本错误；坏帧绝不调用业务回调。
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define PROTOCOL_VERSION            (1U)
#define PROTOCOL_MAX_PAYLOAD_LENGTH (128U)
#define PROTOCOL_FLAG_ACK_REQUIRED  (0x01U)
#define PROTOCOL_FLAG_ERROR         (0x02U)
#define PROTOCOL_FLAG_HIGH_PRIORITY (0x04U)

typedef enum {
    PROTOCOL_MSG_HELLO = 0x01,
    PROTOCOL_MSG_HEARTBEAT = 0x02,
    PROTOCOL_MSG_SET_PARAMS = 0x03,
    PROTOCOL_MSG_GET_STATUS = 0x04,
    PROTOCOL_MSG_ARM = 0x05,
    PROTOCOL_MSG_START_TRIAL = 0x06,
    PROTOCOL_MSG_ABORT_TRIAL = 0x07,
    PROTOCOL_MSG_CLEAR_FAULT = 0x08,
    PROTOCOL_MSG_SET_TELEMETRY = 0x09,
    PROTOCOL_MSG_COMMIT_PARAMS = 0x0A,
    PROTOCOL_MSG_ACK = 0x80,
    PROTOCOL_MSG_NACK = 0x81,
    PROTOCOL_MSG_STATUS = 0x82,
    PROTOCOL_MSG_TELEMETRY = 0x83,
    PROTOCOL_MSG_TRIAL_SUMMARY = 0x84,
    PROTOCOL_MSG_EVENT = 0x85
} ProtocolMessageType;

typedef struct {
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint16_t sequence;
    uint16_t payloadLength;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD_LENGTH];
} ProtocolFrame;

typedef struct {
    uint32_t noiseBytes;
    uint32_t lengthErrors;
    uint32_t crcErrors;
    uint32_t versionErrors;
    uint32_t validFrames;
} ProtocolCounters;

typedef void (*ProtocolFrameHandler)(const ProtocolFrame *frame);

/* 用途：清空解码状态并绑定完整帧回调；不允许 ISR 调用。 */
void Protocol_Init(ProtocolFrameHandler handler);

/* 用途：有界消费当前 UART RX 字节并解码；返回消费字节数；不允许 ISR 调用。 */
uint16_t Protocol_Poll(uint16_t byteBudget);

/*
 * 用途：编码并非阻塞发送一帧。
 * 输出：true=整帧已入 TX 队列，false=参数无效或队列不足。
 * 副作用：可开启 UART TX 中断。ISR 不可调用。
 */
bool Protocol_Send(uint8_t type, uint8_t flags, uint16_t sequence,
    const uint8_t *payload, uint16_t payloadLength, bool highPriority);

/* 用途：返回解码计数快照；无副作用，ISR 不可调用。 */
ProtocolCounters Protocol_GetCounters(void);

#endif /* PROTOCOL_H */
// ----- AI
