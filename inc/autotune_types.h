// ----- AI
/*
 * 1-3 系统跨模块数据类型。
 *
 * 职责：统一状态、故障、停止原因、控制参数和每 tick 样本的含义。
 * 依赖：标准整数和布尔类型。硬件副作用：无。故障输出：由枚举值表达。
 * 缩放：所有增益为有符号 Q16.16；速度单位为每 10 ms 编码器计数。
 */

#ifndef AUTOTUNE_TYPES_H
#define AUTOTUNE_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define Q16_ONE (65536L)

typedef enum {
    SYSTEM_STATE_BOOT_SAFE = 0,
    SYSTEM_STATE_IDLE = 1,
    SYSTEM_STATE_ARMED = 2,
    SYSTEM_STATE_RUNNING = 3,
    SYSTEM_STATE_FINISHED = 4,
    SYSTEM_STATE_FAULT = 5,
    SYSTEM_STATE_FLASH_WRITE = 6
} SystemState;

typedef enum {
    STOP_REASON_NONE = 0,
    STOP_REASON_BOOT = 1,
    STOP_REASON_TRIAL_COMPLETE = 2,
    STOP_REASON_ABORTED = 3,
    STOP_REASON_COMM_TIMEOUT = 4,
    STOP_REASON_LINE_LOST = 5,
    STOP_REASON_STALL_LEFT = 6,
    STOP_REASON_STALL_RIGHT = 7,
    STOP_REASON_CONTROL_OVERRUN = 8,
    STOP_REASON_PARAMETER_ERROR = 9,
    STOP_REASON_FLASH_ERROR = 10
} StopReason;

typedef enum {
    FAULT_NONE = 0,
    FAULT_COMM_TIMEOUT = 1,
    FAULT_LINE_LOST = 2,
    FAULT_STALL_LEFT = 3,
    FAULT_STALL_RIGHT = 4,
    FAULT_CONTROL_OVERRUN = 5,
    FAULT_PARAMETER = 6,
    FAULT_FLASH = 7,
    FAULT_INTERNAL = 8
} FaultCode;

typedef struct {
    int32_t speedKpLeftQ16;
    int32_t speedKiLeftQ16;
    int32_t speedFeedforwardLeftQ16;
    int32_t speedKpRightQ16;
    int32_t speedKiRightQ16;
    int32_t speedFeedforwardRightQ16;
    int32_t lineKpQ16;
    int32_t lineKdQ16;
    int32_t derivativeAlphaQ16;
    int32_t speedIntegralLimit;
    int16_t baseSpeed;
    int16_t maxTargetSpeed;
    int16_t maxDeltaSpeed;
    int16_t maxPwm;
    int16_t derivativeLimit;
    int16_t stallPwmThreshold;
    int16_t stallSpeedThreshold;
    uint16_t telemetryHz;
    uint16_t controlOverrunLimit;
} ControlParams;

typedef struct {
    uint8_t bitmap;
    uint8_t activeCount;
    bool valid;
    bool specialPattern;
    int16_t error;
} LineSensorSample;

typedef struct {
    uint32_t tick;
    LineSensorSample line;
    int16_t leftTarget;
    int16_t rightTarget;
    int16_t leftSpeed;
    int16_t rightSpeed;
    int16_t leftPwm;
    int16_t rightPwm;
    bool targetSaturated;
    bool pwmSaturated;
} ControlSample;

#endif /* AUTOTUNE_TYPES_H */
// ----- AI
