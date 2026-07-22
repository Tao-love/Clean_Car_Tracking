// ----- AI
/*
 * 首版全部命令分发、Session+Sequence 去重、延迟 SET_PARAMS ACK、遥测与汇总。
 * 任何完整 CRC 正确帧都先刷新心跳，但只有会话/载荷/状态合法时才执行命令。
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_protocol.h"
#include "autotune_types.h"
#include "control_params.h"
#include "flash_params.h"
#include "main.h"
#include "protocol.h"
#include "trial_manager.h"
#include "trial_stats.h"

#define FIRMWARE_VERSION_MAJOR (1U)
#define FIRMWARE_VERSION_MINOR (0U)
#define FIRMWARE_VERSION_PATCH (0U)
#define RESPONSE_CACHE_SIZE    (8U)
#define RESPONSE_CACHE_PAYLOAD (24U)
#define SUMMARY_WIRE_SIZE      (128U)

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
    APP_STATUS_BAD_SESSION = 3,
    APP_STATUS_BAD_STATE = 4,
    APP_STATUS_BAD_VERSION = 5,
    APP_STATUS_PARAM_RANGE = 6,
    APP_STATUS_BUSY = 7,
    APP_STATUS_UNSAFE = 8,
    APP_STATUS_INTERNAL = 10
} AppStatus;

typedef struct {
    bool valid;
    uint32_t sessionId;
    uint16_t sequence;
    uint8_t commandType;
    uint8_t responseType;
    uint8_t responseFlags;
    uint8_t payloadLength;
    uint8_t payload[RESPONSE_CACHE_PAYLOAD];
} CachedResponse;

static uint32_t gSessionId;
static CachedResponse gResponseCache[RESPONSE_CACHE_SIZE];
static uint8_t gCacheReplaceIndex;
static bool gPendingParamsAck;
static uint16_t gPendingParamsSequence;
static bool gTelemetryEnabled;
static uint8_t gTelemetryHz;
static uint32_t gLastTelemetryTick;

static void AppProtocol_HandleSessionCommand(const ProtocolFrame *frame);
static bool AppProtocol_ValidateSession(
    const ProtocolFrame *frame, uint16_t minimumPayloadLength);
static bool AppProtocol_ResendCached(const ProtocolFrame *frame);
static void AppProtocol_SendHelloAck(uint16_t sequence);
static void AppProtocol_SendAck(
    const ProtocolFrame *command, AppStatus status);
static void AppProtocol_SendNack(
    const ProtocolFrame *command, AppStatus status);
static void AppProtocol_SendStatus(const ProtocolFrame *command);
static void AppProtocol_SendTelemetry(uint32_t tick);
static void AppProtocol_SendSummary(void);
static void AppProtocol_CacheAndSend(const ProtocolFrame *command,
    uint8_t responseType, uint8_t responseFlags,
    const uint8_t *payload, uint8_t payloadLength);
static AppStatus AppProtocol_MapTrialResult(TrialCommandResult result);
static AppStatus AppProtocol_MapParamsResult(ParamsValidationResult result);
static uint16_t AppProtocol_ReadU16(const uint8_t *source);
static uint32_t AppProtocol_ReadU32(const uint8_t *source);
static void AppProtocol_WriteU16(uint8_t *destination, uint16_t value);
static void AppProtocol_WriteU32(uint8_t *destination, uint32_t value);
static void AppProtocol_WriteI16(uint8_t *destination, int16_t value);
static void AppProtocol_WriteI32(uint8_t *destination, int32_t value);
static void AppProtocol_WriteI64(uint8_t *destination, int64_t value);

void AppProtocol_Init(uint32_t sessionId)
{
    gSessionId = sessionId;
    (void) memset(gResponseCache, 0, sizeof(gResponseCache));
    gCacheReplaceIndex = 0U;
    gPendingParamsAck = false;
    gPendingParamsSequence = 0U;
    gTelemetryEnabled = true;
    gTelemetryHz = 10U;
    gLastTelemetryTick = 0U;
}

void AppProtocol_HandleFrame(const ProtocolFrame *frame)
{
    if (frame == 0) {
        return;
    }
    if (frame->version != PROTOCOL_VERSION) {
        AppProtocol_SendNack(frame, APP_STATUS_BAD_VERSION);
        return;
    }
    if (frame->type == PROTOCOL_MSG_HELLO) {
        if (frame->payloadLength == 0U) {
            TrialManager_OnValidFrame(gControlTick);
            AppProtocol_SendHelloAck(frame->sequence);
        } else {
            AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
        }
        return;
    }
    AppProtocol_HandleSessionCommand(frame);
}

void AppProtocol_OnControlBoundary(uint32_t tick)
{
    TrialManagerStatus status = TrialManager_GetStatus();

    if (gPendingParamsAck && TrialManager_ConsumeParamsApplied()) {
        ProtocolFrame command;

        (void) memset(&command, 0, sizeof(command));
        command.type = PROTOCOL_MSG_SET_PARAMS;
        command.sequence = gPendingParamsSequence;
        AppProtocol_SendAck(&command, APP_STATUS_OK);
        gPendingParamsAck = false;
    }

    if (status.summaryReady) {
        AppProtocol_SendSummary();
    }

    if (gTelemetryEnabled && (gTelemetryHz != 0U)) {
        uint32_t interval = 100U / gTelemetryHz;
        if (interval == 0U) {
            interval = 1U;
        }
        if ((uint32_t) (tick - gLastTelemetryTick) >= interval) {
            AppProtocol_SendTelemetry(tick);
            gLastTelemetryTick = tick;
        }
    }
}

static void AppProtocol_HandleSessionCommand(const ProtocolFrame *frame)
{
    if (!AppProtocol_ValidateSession(frame, 4U)) {
        return;
    }
    TrialManager_OnValidFrame(gControlTick);
    if (AppProtocol_ResendCached(frame)) {
        return;
    }
    if (gPendingParamsAck && (frame->type == PROTOCOL_MSG_SET_PARAMS) &&
        (frame->sequence == gPendingParamsSequence)) {
        return;
    }

    switch (frame->type) {
        case PROTOCOL_MSG_HEARTBEAT:
            if (frame->payloadLength == 4U) {
                AppProtocol_SendAck(frame, APP_STATUS_OK);
            } else {
                AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
            }
            break;
        case PROTOCOL_MSG_SET_PARAMS:
            if (frame->payloadLength == (4U + CONTROL_PARAMS_WIRE_SIZE)) {
                ControlParams params;
                ParamsValidationResult result;

                if (!ControlParams_DecodeWire(&frame->payload[4],
                        CONTROL_PARAMS_WIRE_SIZE, &params)) {
                    AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
                    break;
                }
                result = TrialManager_StageParams(&params);
                if (result == PARAMS_VALID) {
                    gPendingParamsAck = true;
                    gPendingParamsSequence = frame->sequence;
                } else {
                    AppProtocol_SendNack(
                        frame, AppProtocol_MapParamsResult(result));
                }
            } else {
                AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
            }
            break;
        case PROTOCOL_MSG_GET_STATUS:
            if (frame->payloadLength == 4U) {
                AppProtocol_SendStatus(frame);
            } else {
                AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
            }
            break;
        case PROTOCOL_MSG_ARM:
            if (frame->payloadLength == 6U) {
                TrialCommandResult result =
                    TrialManager_Arm(AppProtocol_ReadU16(&frame->payload[4]));
                AppStatus status = AppProtocol_MapTrialResult(result);
                if (status == APP_STATUS_OK) {
                    AppProtocol_SendAck(frame, status);
                } else {
                    AppProtocol_SendNack(frame, status);
                }
            } else {
                AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
            }
            break;
        case PROTOCOL_MSG_START_TRIAL:
            if ((frame->payloadLength == 4U) ||
                (frame->payloadLength == 9U)) {
                TrialMode mode = TRIAL_MODE_LINE_FOLLOW;
                int16_t leftCommand = 0;
                int16_t rightCommand = 0;
                if (frame->payloadLength == 9U) {
                    mode = (TrialMode) frame->payload[4];
                    leftCommand = (int16_t) AppProtocol_ReadU16(&frame->payload[5]);
                    rightCommand = (int16_t) AppProtocol_ReadU16(&frame->payload[7]);
                }
                AppStatus status = AppProtocol_MapTrialResult(
                    TrialManager_Start(
                        gControlTick, mode, leftCommand, rightCommand,
                        TRIAL_START_SOURCE_BLUETOOTH));
                if (status == APP_STATUS_OK) {
                    AppProtocol_SendAck(frame, status);
                } else {
                    AppProtocol_SendNack(frame, status);
                }
            } else {
                AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
            }
            break;
        case PROTOCOL_MSG_ABORT_TRIAL:
            if (frame->payloadLength == 4U) {
                AppStatus status = AppProtocol_MapTrialResult(
                    TrialManager_Abort(gControlTick));
                if (status == APP_STATUS_OK) {
                    AppProtocol_SendAck(frame, status);
                } else {
                    AppProtocol_SendNack(frame, status);
                }
            } else {
                AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
            }
            break;
        case PROTOCOL_MSG_CLEAR_FAULT:
            if (frame->payloadLength == 4U) {
                AppStatus status = AppProtocol_MapTrialResult(
                    TrialManager_ClearFault(gControlTick));
                if (status == APP_STATUS_OK) {
                    AppProtocol_SendAck(frame, status);
                } else {
                    AppProtocol_SendNack(frame, status);
                }
            } else {
                AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
            }
            break;
        case PROTOCOL_MSG_SET_TELEMETRY:
            if ((frame->payloadLength == 6U) && (frame->payload[5] <= 20U)) {
                gTelemetryEnabled = frame->payload[4] != 0U;
                gTelemetryHz = frame->payload[5];
                AppProtocol_SendAck(frame, APP_STATUS_OK);
            } else {
                AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
            }
            break;
        case PROTOCOL_MSG_COMMIT_PARAMS:
            if (frame->payloadLength == 6U) {
                uint16_t version = AppProtocol_ReadU16(&frame->payload[4]);
                TrialCommandResult begin = TrialManager_BeginFlashWrite(
                    gControlTick, version);
                if (begin == TRIAL_COMMAND_OK) {
                    FlashParamsResult flashResult = FlashParams_Commit(
                        ControlParams_GetActive(), version);
                    bool success = (flashResult == FLASH_PARAMS_OK) ||
                        (flashResult == FLASH_PARAMS_ALREADY_CURRENT);
                    TrialManager_EndFlashWrite(gControlTick, success);
                    if (success) {
                        AppProtocol_SendAck(frame, APP_STATUS_OK);
                    } else {
                        AppProtocol_SendNack(frame, APP_STATUS_INTERNAL);
                    }
                } else {
                    AppProtocol_SendNack(
                        frame, AppProtocol_MapTrialResult(begin));
                }
            } else {
                AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
            }
            break;
        default:
            AppProtocol_SendNack(frame, APP_STATUS_UNKNOWN_MESSAGE);
            break;
    }
}

static bool AppProtocol_ValidateSession(
    const ProtocolFrame *frame, uint16_t minimumPayloadLength)
{
    if (frame->payloadLength < minimumPayloadLength) {
        AppProtocol_SendNack(frame, APP_STATUS_BAD_PAYLOAD);
        return false;
    }
    if (AppProtocol_ReadU32(frame->payload) != gSessionId) {
        AppProtocol_SendNack(frame, APP_STATUS_BAD_SESSION);
        return false;
    }
    return true;
}

static bool AppProtocol_ResendCached(const ProtocolFrame *frame)
{
    uint8_t index;

    for (index = 0U; index < RESPONSE_CACHE_SIZE; index++) {
        const CachedResponse *cached = &gResponseCache[index];
        if (cached->valid && (cached->sessionId == gSessionId) &&
            (cached->sequence == frame->sequence) &&
            (cached->commandType == frame->type)) {
            (void) Protocol_Send(cached->responseType, cached->responseFlags,
                frame->sequence, cached->payload, cached->payloadLength, true);
            return true;
        }
    }
    return false;
}

static void AppProtocol_SendHelloAck(uint16_t sequence)
{
    uint8_t payload[16];
    TrialManagerStatus status = TrialManager_GetStatus();

    payload[0] = PROTOCOL_MSG_HELLO;
    payload[1] = APP_STATUS_OK;
    AppProtocol_WriteU16(&payload[2], status.paramVersion);
    AppProtocol_WriteU32(&payload[4], gSessionId);
    payload[8] = PROTOCOL_VERSION;
    payload[9] = FIRMWARE_VERSION_MAJOR;
    payload[10] = FIRMWARE_VERSION_MINOR;
    payload[11] = FIRMWARE_VERSION_PATCH;
    AppProtocol_WriteU32(&payload[12], CAPABILITIES_ALL);
    (void) Protocol_Send(PROTOCOL_MSG_ACK, PROTOCOL_FLAG_HIGH_PRIORITY,
        sequence, payload, sizeof(payload), true);
}

static void AppProtocol_SendAck(
    const ProtocolFrame *command, AppStatus status)
{
    uint8_t payload[8];

    payload[0] = command->type;
    payload[1] = (uint8_t) status;
    AppProtocol_WriteU16(&payload[2], ControlParams_GetVersion());
    AppProtocol_WriteU32(&payload[4], gSessionId);
    AppProtocol_CacheAndSend(command, PROTOCOL_MSG_ACK,
        PROTOCOL_FLAG_HIGH_PRIORITY, payload, sizeof(payload));
}

static void AppProtocol_SendNack(
    const ProtocolFrame *command, AppStatus status)
{
    uint8_t payload[8];

    payload[0] = command->type;
    payload[1] = (uint8_t) status;
    AppProtocol_WriteU16(&payload[2], ControlParams_GetVersion());
    AppProtocol_WriteU32(&payload[4], gSessionId);
    AppProtocol_CacheAndSend(command, PROTOCOL_MSG_NACK,
        PROTOCOL_FLAG_ERROR | PROTOCOL_FLAG_HIGH_PRIORITY,
        payload, sizeof(payload));
}

static void AppProtocol_SendStatus(const ProtocolFrame *command)
{
    uint8_t payload[20];
    TrialManagerStatus status = TrialManager_GetStatus();

    payload[0] = (uint8_t) status.state;
    payload[1] = (uint8_t) status.safety.fault;
    payload[2] = (uint8_t) status.safety.stopReason;
    payload[3] = status.summaryReady ? 1U : 0U;
    AppProtocol_WriteU16(&payload[4], status.paramVersion);
    AppProtocol_WriteU16(&payload[6], status.trialTicks);
    AppProtocol_WriteU32(&payload[8], status.controlTick);
    AppProtocol_WriteU32(&payload[12], status.controlOverruns);
    AppProtocol_WriteU32(&payload[16], gSessionId);
    AppProtocol_CacheAndSend(command, PROTOCOL_MSG_STATUS,
        PROTOCOL_FLAG_HIGH_PRIORITY, payload, sizeof(payload));
}

static void AppProtocol_SendTelemetry(uint32_t tick)
{
    uint8_t payload[24];
    TrialManagerStatus status = TrialManager_GetStatus();

    AppProtocol_WriteU32(&payload[0], tick);
    payload[4] = status.latestSample.line.bitmap;
    payload[5] = (uint8_t) status.state;
    payload[6] = (uint8_t) status.safety.fault;
    payload[7] = status.latestSample.line.valid ? 1U : 0U;
    AppProtocol_WriteI16(&payload[8], status.latestSample.line.error);
    AppProtocol_WriteI16(&payload[10], status.latestSample.leftTarget);
    AppProtocol_WriteI16(&payload[12], status.latestSample.rightTarget);
    AppProtocol_WriteI16(&payload[14], status.latestSample.leftSpeed);
    AppProtocol_WriteI16(&payload[16], status.latestSample.rightSpeed);
    AppProtocol_WriteI16(&payload[18], status.latestSample.leftPwm);
    AppProtocol_WriteI16(&payload[20], status.latestSample.rightPwm);
    AppProtocol_WriteU16(&payload[22], status.paramVersion);
    (void) Protocol_Send(PROTOCOL_MSG_TELEMETRY, 0U, 0U,
        payload, sizeof(payload), false);
}

static void AppProtocol_SendSummary(void)
{
    TrialSummary summary;
    uint8_t payload[SUMMARY_WIRE_SIZE];
    uint8_t *cursor = payload;

    if (!TrialManager_GetSummary(&summary)) {
        return;
    }
#define PUT_U16(value) do { AppProtocol_WriteU16(cursor, (uint16_t) (value)); cursor += 2; } while (0)
#define PUT_I16(value) do { AppProtocol_WriteI16(cursor, (int16_t) (value)); cursor += 2; } while (0)
#define PUT_I32(value) do { AppProtocol_WriteI32(cursor, (int32_t) (value)); cursor += 4; } while (0)
#define PUT_I64(value) do { AppProtocol_WriteI64(cursor, (int64_t) (value)); cursor += 8; } while (0)
    PUT_U16(summary.paramVersion);
    PUT_U16(summary.sampleCount);
    PUT_U16(summary.runTicks);
    *cursor++ = (uint8_t) summary.stopReason;
    *cursor++ = (uint8_t) summary.fault;
    PUT_U16(summary.arithmeticSaturated ? 1U : 0U);
    PUT_U16(summary.lostSamples);
    PUT_U16(summary.longestLostTicks);
    PUT_U16(summary.targetSaturationSamples);
    PUT_U16(summary.pwmSaturationSamples);
    PUT_U16(summary.signFlips);
    PUT_U16(summary.controlOverruns);
    PUT_U16(summary.specialPatternSamples);
    PUT_I16(summary.maxAbsError);
    PUT_I16(summary.approximateP95Error);
    PUT_I16(summary.maxAbsLeftPwm);
    PUT_I16(summary.maxAbsRightPwm);
    PUT_I16(summary.maxAbsLeftTarget);
    PUT_I16(summary.maxAbsRightTarget);
    PUT_I16(summary.maxAbsLeftSpeed);
    PUT_I16(summary.maxAbsRightSpeed);
    PUT_I32(summary.leftEncoderCounts);
    PUT_I32(summary.rightEncoderCounts);
    PUT_I64(summary.absErrorSum);
    PUT_I64(summary.squaredErrorSum);
    PUT_I64(summary.leftTargetSum);
    PUT_I64(summary.rightTargetSum);
    PUT_I64(summary.leftSpeedSum);
    PUT_I64(summary.rightSpeedSum);
    PUT_I64(summary.leftAbsPwmSum);
    PUT_I64(summary.rightAbsPwmSum);
    PUT_I64(summary.speedAbsErrorSum);
    PUT_I64(summary.speedSquaredErrorSum);
#undef PUT_U16
#undef PUT_I16
#undef PUT_I32
#undef PUT_I64
    if (Protocol_Send(PROTOCOL_MSG_TRIAL_SUMMARY,
            PROTOCOL_FLAG_HIGH_PRIORITY, 0U,
            payload, (uint16_t) (cursor - payload), true)) {
        TrialManager_MarkSummaryQueued();
    }
}

static void AppProtocol_CacheAndSend(const ProtocolFrame *command,
    uint8_t responseType, uint8_t responseFlags,
    const uint8_t *payload, uint8_t payloadLength)
{
    CachedResponse *cached = &gResponseCache[gCacheReplaceIndex];

    if (payloadLength <= RESPONSE_CACHE_PAYLOAD) {
        cached->valid = true;
        cached->sessionId = gSessionId;
        cached->sequence = command->sequence;
        cached->commandType = command->type;
        cached->responseType = responseType;
        cached->responseFlags = responseFlags;
        cached->payloadLength = payloadLength;
        (void) memcpy(cached->payload, payload, payloadLength);
        gCacheReplaceIndex = (uint8_t) ((gCacheReplaceIndex + 1U) % RESPONSE_CACHE_SIZE);
    }
    (void) Protocol_Send(responseType, responseFlags, command->sequence,
        payload, payloadLength, true);
}

static AppStatus AppProtocol_MapTrialResult(TrialCommandResult result)
{
    switch (result) {
        case TRIAL_COMMAND_OK: return APP_STATUS_OK;
        case TRIAL_COMMAND_BAD_STATE: return APP_STATUS_BAD_STATE;
        case TRIAL_COMMAND_BAD_VERSION: return APP_STATUS_BAD_VERSION;
        case TRIAL_COMMAND_BAD_PARAMS: return APP_STATUS_PARAM_RANGE;
        case TRIAL_COMMAND_CONDITION_UNSAFE: return APP_STATUS_UNSAFE;
        default: return APP_STATUS_INTERNAL;
    }
}

static AppStatus AppProtocol_MapParamsResult(ParamsValidationResult result)
{
    switch (result) {
        case PARAMS_VALID: return APP_STATUS_OK;
        case PARAMS_RUNNING_LOCKED: return APP_STATUS_BAD_STATE;
        case PARAMS_ALREADY_PENDING: return APP_STATUS_BUSY;
        case PARAMS_NULL:
        case PARAMS_GAIN_RANGE:
        case PARAMS_LIMIT_RANGE:
        case PARAMS_RELATION_INVALID:
            return APP_STATUS_PARAM_RANGE;
        default: return APP_STATUS_INTERNAL;
    }
}

static uint16_t AppProtocol_ReadU16(const uint8_t *source)
{
    return (uint16_t) source[0] | ((uint16_t) source[1] << 8);
}

static uint32_t AppProtocol_ReadU32(const uint8_t *source)
{
    return (uint32_t) source[0] | ((uint32_t) source[1] << 8) |
        ((uint32_t) source[2] << 16) | ((uint32_t) source[3] << 24);
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

static void AppProtocol_WriteI16(uint8_t *destination, int16_t value)
{
    AppProtocol_WriteU16(destination, (uint16_t) value);
}

static void AppProtocol_WriteI32(uint8_t *destination, int32_t value)
{
    AppProtocol_WriteU32(destination, (uint32_t) value);
}

static void AppProtocol_WriteI64(uint8_t *destination, int64_t value)
{
    uint64_t raw = (uint64_t) value;
    uint8_t index;
    for (index = 0U; index < 8U; index++) {
        destination[index] = (uint8_t) (raw >> (index * 8U));
    }
}
// ----- AI
