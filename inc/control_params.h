// ----- AI
/*
 * 运行时参数双缓冲模块。
 * 职责：校验候选、保存 pending/active，并只在非 RUNNING 的 10 ms 边界整组切换。
 * 依赖：autotune_types。上下文：主循环。硬件副作用：无。
 * 故障输出：返回明确校验结果；从不部分覆盖 activeParams。
 */

#ifndef CONTROL_PARAMS_H
#define CONTROL_PARAMS_H

#include <stdbool.h>
#include <stdint.h>

#include "autotune_types.h"

#define CONTROL_PARAMS_WIRE_SIZE (58U)

typedef enum {
    PARAMS_VALID = 0,
    PARAMS_NULL = 1,
    PARAMS_GAIN_RANGE = 2,
    PARAMS_LIMIT_RANGE = 3,
    PARAMS_RELATION_INVALID = 4,
    PARAMS_RUNNING_LOCKED = 5,
    PARAMS_ALREADY_PENDING = 6
} ParamsValidationResult;

/* 用途：加载保守默认值（baseSpeed=0）并清空 pending；不允许 ISR 调用。 */
void ControlParams_Init(const ControlParams *flashParams);

/* 用途：只校验完整参数结构；无副作用，ISR 不可调用。 */
ParamsValidationResult ControlParams_Validate(const ControlParams *params);

/* 用途：将已校验候选整组放入 pending；RUNNING 时拒绝；不允许 ISR 调用。 */
ParamsValidationResult ControlParams_Stage(
    const ControlParams *params, SystemState state);

/* 用途：在 10 ms 控制边界原子应用 pending；返回是否切换；RUNNING 时不动作。 */
bool ControlParams_ApplyPendingAtBoundary(SystemState state);

/* 用途：返回只读 active 指针；指针始终有效；ISR 不可调用。 */
const ControlParams *ControlParams_GetActive(void);

/* 用途：返回每次成功切换后递增的版本；无副作用。 */
uint16_t ControlParams_GetVersion(void);

/* 用途：检查是否存在待应用候选；无副作用。 */
bool ControlParams_HasPending(void);

/* 用途：按固定 58 字节小端协议解码；长度或指针错返回 false。 */
bool ControlParams_DecodeWire(
    const uint8_t *payload, uint16_t length, ControlParams *params);

/* 用途：按固定 58 字节小端协议编码；指针错返回 false。 */
bool ControlParams_EncodeWire(
    const ControlParams *params, uint8_t *payload, uint16_t capacity);

#endif /* CONTROL_PARAMS_H */
// ----- AI
