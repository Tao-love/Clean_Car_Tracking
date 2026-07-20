// ----- AI
/* 协议流式解码与帧发送实现。 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "crc16.h"
#include "protocol.h"
#include "uart_transport.h"

#define PROTOCOL_MAGIC_0          (0xA5U)
#define PROTOCOL_MAGIC_1          (0x5AU)
#define PROTOCOL_FIXED_BODY_SIZE  (7U)
#define PROTOCOL_HEADER_SIZE      (9U)
#define PROTOCOL_CRC_SIZE         (2U)
#define PROTOCOL_MAX_FRAME_LENGTH \
    (PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_LENGTH + PROTOCOL_CRC_SIZE)

static uint8_t gReceiveFrame[PROTOCOL_MAX_FRAME_LENGTH];
static uint16_t gReceiveLength;
static uint16_t gExpectedLength;
static ProtocolFrameHandler gFrameHandler;
static ProtocolCounters gCounters;

static void Protocol_ConsumeByte(uint8_t value);
static void Protocol_ResetDecoder(void);
static void Protocol_WriteU16(uint8_t *destination, uint16_t value);
static uint16_t Protocol_ReadU16(const uint8_t *source);

void Protocol_Init(ProtocolFrameHandler handler)
{
    Protocol_ResetDecoder();
    gFrameHandler = handler;
    (void) memset(&gCounters, 0, sizeof(gCounters));
}

uint16_t Protocol_Poll(uint16_t byteBudget)
{
    uint16_t consumed = 0U;
    uint8_t value;

    while ((consumed < byteBudget) && UART_Transport_ReadByte(&value)) {
        Protocol_ConsumeByte(value);
        consumed++;
    }
    return consumed;
}

bool Protocol_Send(uint8_t type, uint8_t flags, uint16_t sequence,
    const uint8_t *payload, uint16_t payloadLength, bool highPriority)
{
    uint8_t encoded[PROTOCOL_MAX_FRAME_LENGTH];
    uint16_t bodyLength;
    uint16_t crc;

    if ((payloadLength > PROTOCOL_MAX_PAYLOAD_LENGTH) ||
        ((payload == 0) && (payloadLength != 0U))) {
        return false;
    }

    encoded[0] = PROTOCOL_MAGIC_0;
    encoded[1] = PROTOCOL_MAGIC_1;
    encoded[2] = PROTOCOL_VERSION;
    encoded[3] = type;
    encoded[4] = flags;
    Protocol_WriteU16(&encoded[5], sequence);
    Protocol_WriteU16(&encoded[7], payloadLength);
    if (payloadLength != 0U) {
        (void) memcpy(&encoded[9], payload, payloadLength);
    }
    bodyLength = (uint16_t) (PROTOCOL_FIXED_BODY_SIZE + payloadLength);
    crc = CRC16_Calculate(&encoded[2], bodyLength);
    Protocol_WriteU16(&encoded[9U + payloadLength], crc);
    return UART_Transport_Write(encoded,
        (uint16_t) (PROTOCOL_HEADER_SIZE + payloadLength + PROTOCOL_CRC_SIZE),
        highPriority);
}

ProtocolCounters Protocol_GetCounters(void)
{
    return gCounters;
}

static void Protocol_ConsumeByte(uint8_t value)
{
    if (gReceiveLength == 0U) {
        if (value == PROTOCOL_MAGIC_0) {
            gReceiveFrame[gReceiveLength++] = value;
        } else {
            gCounters.noiseBytes++;
        }
        return;
    }

    if (gReceiveLength == 1U) {
        if (value == PROTOCOL_MAGIC_1) {
            gReceiveFrame[gReceiveLength++] = value;
        } else if (value == PROTOCOL_MAGIC_0) {
            gCounters.noiseBytes++;
            gReceiveFrame[0] = value;
        } else {
            gCounters.noiseBytes += 2U;
            Protocol_ResetDecoder();
        }
        return;
    }

    gReceiveFrame[gReceiveLength++] = value;
    if (gReceiveLength == PROTOCOL_HEADER_SIZE) {
        uint16_t payloadLength = Protocol_ReadU16(&gReceiveFrame[7]);

        if (payloadLength > PROTOCOL_MAX_PAYLOAD_LENGTH) {
            gCounters.lengthErrors++;
            Protocol_ResetDecoder();
            return;
        }
        gExpectedLength = (uint16_t) (
            PROTOCOL_HEADER_SIZE + payloadLength + PROTOCOL_CRC_SIZE);
    }

    if ((gExpectedLength != 0U) && (gReceiveLength == gExpectedLength)) {
        uint16_t payloadLength = Protocol_ReadU16(&gReceiveFrame[7]);
        uint16_t receivedCrc = Protocol_ReadU16(&gReceiveFrame[9U + payloadLength]);
        uint16_t calculatedCrc = CRC16_Calculate(
            &gReceiveFrame[2], (uint16_t) (PROTOCOL_FIXED_BODY_SIZE + payloadLength));

        if (receivedCrc != calculatedCrc) {
            gCounters.crcErrors++;
        } else if (gFrameHandler != 0) {
            ProtocolFrame frame;

            frame.version = gReceiveFrame[2];
            frame.type = gReceiveFrame[3];
            frame.flags = gReceiveFrame[4];
            frame.sequence = Protocol_ReadU16(&gReceiveFrame[5]);
            frame.payloadLength = payloadLength;
            if (payloadLength != 0U) {
                (void) memcpy(frame.payload, &gReceiveFrame[9], payloadLength);
            }
            if (frame.version != PROTOCOL_VERSION) {
                gCounters.versionErrors++;
            } else {
                gCounters.validFrames++;
            }
            gFrameHandler(&frame);
        }
        Protocol_ResetDecoder();
    }
}

static void Protocol_ResetDecoder(void)
{
    gReceiveLength = 0U;
    gExpectedLength = 0U;
}

static void Protocol_WriteU16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t) value;
    destination[1] = (uint8_t) (value >> 8);
}

static uint16_t Protocol_ReadU16(const uint8_t *source)
{
    return (uint16_t) source[0] | ((uint16_t) source[1] << 8);
}
// ----- AI
