// ----- AI
/* HELLO 阶段的业务命令分发。 */

#include <stdint.h>

#include "app_protocol.h"
#include "protocol.h"

#define FIRMWARE_VERSION_MAJOR (1U)
#define FIRMWARE_VERSION_MINOR (0U)
#define FIRMWARE_VERSION_PATCH (0U)

#define CAPABILITY_RUNTIME_PARAMS (1UL << 0)
#define CAPABILITY_TRIAL_5_SECONDS (1UL << 1)
#define CAPABILITY_LOCAL_SAFETY   (1UL << 2)
#define CAPABILITY_LOCAL_STATS    (1UL << 3)
#define CAPABILITY_FLASH_DUAL     (1UL << 4)
#define CAPABILITY_TELEMETRY      (1UL << 5)
#define CAPABILITIES_ALL (CAPABILITY_RUNTIME_PARAMS | CAPABILITY_TRIAL_5_SECONDS | \
    CAPABILITY_LOCAL_SAFETY | CAPABILITY_LOCAL_STATS | CAPABILITY_FLASH_DUAL | \
    CAPABILITY_TELEMETRY)

typedef enum {
    APP_STATUS_OK = 0,
    APP_STATUS_UNKNOWN_MESSAGE = 1,
    APP_STATUS_BAD_PAYLOAD = 2,
    APP_STATUS_NOT_IMPLEMENTED = 3
} AppStatus;

static uint32_t gSessionId;
static uint16_t gParamVersion;

static void AppProtocol_SendHelloAck(uint16_t sequence);
static void AppProtocol_SendNack(
    uint16_t sequence, uint8_t acknowledgedType, AppStatus status);
static void AppProtocol_WriteU16(uint8_t *destination, uint16_t value);
static void AppProtocol_WriteU32(uint8_t *destination, uint32_t value);

void AppProtocol_Init(uint32_t sessionId)
{
    gSessionId = sessionId;
    gParamVersion = 0U;
}

void AppProtocol_HandleFrame(const ProtocolFrame *frame)
{
    if (frame == 0) {
        return;
    }

    switch (frame->type) {
        case PROTOCOL_MSG_HELLO:
            if (frame->payloadLength == 0U) {
                AppProtocol_SendHelloAck(frame->sequence);
            } else {
                AppProtocol_SendNack(
                    frame->sequence, frame->type, APP_STATUS_BAD_PAYLOAD);
            }
            break;
        case PROTOCOL_MSG_HEARTBEAT:
        case PROTOCOL_MSG_SET_PARAMS:
        case PROTOCOL_MSG_GET_STATUS:
        case PROTOCOL_MSG_ARM:
        case PROTOCOL_MSG_START_TRIAL:
        case PROTOCOL_MSG_ABORT_TRIAL:
        case PROTOCOL_MSG_CLEAR_FAULT:
        case PROTOCOL_MSG_SET_TELEMETRY:
        case PROTOCOL_MSG_COMMIT_PARAMS:
            AppProtocol_SendNack(
                frame->sequence, frame->type, APP_STATUS_NOT_IMPLEMENTED);
            break;
        default:
            AppProtocol_SendNack(
                frame->sequence, frame->type, APP_STATUS_UNKNOWN_MESSAGE);
            break;
    }
}

static void AppProtocol_SendHelloAck(uint16_t sequence)
{
    uint8_t payload[16];

    payload[0] = PROTOCOL_MSG_HELLO;
    payload[1] = APP_STATUS_OK;
    AppProtocol_WriteU16(&payload[2], gParamVersion);
    AppProtocol_WriteU32(&payload[4], gSessionId);
    payload[8] = PROTOCOL_VERSION;
    payload[9] = FIRMWARE_VERSION_MAJOR;
    payload[10] = FIRMWARE_VERSION_MINOR;
    payload[11] = FIRMWARE_VERSION_PATCH;
    AppProtocol_WriteU32(&payload[12], CAPABILITIES_ALL);
    (void) Protocol_Send(PROTOCOL_MSG_ACK, PROTOCOL_FLAG_HIGH_PRIORITY,
        sequence, payload, sizeof(payload), true);
}

static void AppProtocol_SendNack(
    uint16_t sequence, uint8_t acknowledgedType, AppStatus status)
{
    uint8_t payload[4];

    payload[0] = acknowledgedType;
    payload[1] = (uint8_t) status;
    AppProtocol_WriteU16(&payload[2], gParamVersion);
    (void) Protocol_Send(PROTOCOL_MSG_NACK,
        PROTOCOL_FLAG_ERROR | PROTOCOL_FLAG_HIGH_PRIORITY,
        sequence, payload, sizeof(payload), true);
}

static void AppProtocol_WriteU16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t) value;
    destination[1] = (uint8_t) (value >> 8);
}

static void AppProtocol_WriteU32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t) value;
    destination[1] = (uint8_t) (value >> 8);
    destination[2] = (uint8_t) (value >> 16);
    destination[3] = (uint8_t) (value >> 24);
}
// ----- AI
