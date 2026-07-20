// ----- AI
/*
 * 5 秒试验状态机和 100 Hz 控制编排模块。
 * 职责：管理 BOOT_SAFE/IDLE/ARMED/RUNNING/FINISHED/FAULT/FLASH_WRITE，调用灰度、速度 PI、循线 PD、安全和统计。
 * 依赖：adc、encoder、motor、control_params、line_control、speed_pi、safety_guard、trial_stats。
 * 上下文：所有 API 只由主循环调用，禁止 ISR。单位：tick=10 ms，trial 最多 500 tick。
 * 硬件副作用：RUNNING 时写电机，所有退出路径均经统一停车。
 * 故障输出：FAULT 状态、SafetyStatus 和 TrialSummary。
 */

#ifndef TRIAL_MANAGER_H
#define TRIAL_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "autotune_types.h"
#include "control_params.h"
#include "safety_guard.h"
#include "trial_stats.h"

#define TRIAL_DURATION_TICKS (500U)

typedef enum {
    TRIAL_COMMAND_OK = 0,
    TRIAL_COMMAND_BAD_STATE = 1,
    TRIAL_COMMAND_BAD_VERSION = 2,
    TRIAL_COMMAND_BAD_PARAMS = 3,
    TRIAL_COMMAND_CONDITION_UNSAFE = 4
} TrialCommandResult;

typedef struct {
    SystemState state;
    uint16_t paramVersion;
    uint16_t trialTicks;
    uint32_t controlTick;
    uint32_t controlOverruns;
    SafetyStatus safety;
    ControlSample latestSample;
    bool summaryReady;
} TrialManagerStatus;

/* 用途：初始化全部控制状态，执行 BOOT_SAFE 停车后进入 IDLE；禁止 ISR。 */
void TrialManager_Init(uint32_t tick, const ControlParams *flashParams);

/* 用途：执行一个且仅一个 10 ms 边界；RUNNING 时更新双闭环、安全和统计。 */
void TrialManager_ControlTick(uint32_t tick);

/* 用途：通知完整 CRC 正确帧到达，刷新本地心跳。 */
void TrialManager_OnValidFrame(uint32_t tick);

/* 用途：通知主循环漏执行的 tick 数；RUNNING 时可触发本地故障。 */
void TrialManager_ReportOverrun(uint32_t tick, uint16_t missedTicks);

/* 用途：暂存完整候选参数；RUNNING 或已有 pending 时拒绝。 */
ParamsValidationResult TrialManager_StageParams(const ControlParams *params);

/* 用途：检查参数版本和安全条件后 IDLE→ARMED。 */
TrialCommandResult TrialManager_Arm(uint16_t paramVersion);

/* 用途：ARMED→RUNNING，开始独立最多 500 tick 试验。 */
TrialCommandResult TrialManager_Start(uint32_t tick);

/* 用途：人工提前结束 RUNNING，统一停车并生成汇总。 */
TrialCommandResult TrialManager_Abort(uint32_t tick);

/* 用途：条件恢复后 FAULT→IDLE，不会自动重启。 */
TrialCommandResult TrialManager_ClearFault(uint32_t tick);

/* 用途：返回状态快照。 */
TrialManagerStatus TrialManager_GetStatus(void);

/* 用途：读取稳定汇总；仅 summaryReady 时返回 true。 */
bool TrialManager_GetSummary(TrialSummary *summary);

/* 用途：汇总整帧进入高优先级 TX 队列后确认，成功试验 FINISHED→IDLE。 */
void TrialManager_MarkSummaryQueued(void);

/* 用途：返回并清除“本边界已应用新参数”通知，用于延迟 SET_PARAMS ACK。 */
bool TrialManager_ConsumeParamsApplied(void);

#endif /* TRIAL_MANAGER_H */
// ----- AI
