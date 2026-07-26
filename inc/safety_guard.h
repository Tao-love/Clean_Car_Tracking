// ----- AI
/*
 * 本地安全联锁与统一停车模块。
 * 职责：在 MCU 本地以 6.667 ms 粒度判定丢线、左右堵转和控制超期，并执行唯一停车路径。
 * 依赖：motor、ControlParams、ControlSample。上下文：150 Hz 主循环任务。
 * 单位：所有持续时间用 6.667 ms tick。硬件副作用：故障时立即清 PWM 和四方向脚。
 * 故障输出：锁存 StopReason/FaultCode/发生 tick，清除故障不会自动重启。
 */

#ifndef SAFETY_GUARD_H
#define SAFETY_GUARD_H

#include <stdbool.h>
#include <stdint.h>

#include "autotune_types.h"

#define SAFETY_LINE_LOST_TICKS    (23U)
#define SAFETY_STALL_TICKS        (45U)

typedef void (*SafetyControlResetHandler)(void);

typedef struct {
    StopReason stopReason;
    FaultCode fault;
    uint32_t stopTick;
    uint16_t lineLostTicks;
    uint16_t leftStallTicks;
    uint16_t rightStallTicks;
    uint16_t consecutiveOverruns;
} SafetyStatus;

/* 用途：绑定控制状态清零回调并执行上电停车；禁止 ISR 调用。 */
void SafetyGuard_Init(SafetyControlResetHandler resetHandler, uint32_t tick);

/* 用途：在 trial 开始前清本地保护计时器；不清除未确认的故障。 */
void SafetyGuard_BeginTrial(uint32_t tick);

/* 用途：执行一次本地联锁判定；requireLine=false 时跳过丢线检查。 */
bool SafetyGuard_Evaluate(uint32_t tick, const ControlSample *sample,
    const ControlParams *params, bool requireLine);

/* 用途：记录主循环漏执行；连续达到参数限值时本地故障停车。 */
bool SafetyGuard_ReportOverrun(
    uint32_t tick, uint16_t missedTicks, const ControlParams *params);

/* 用途：在没有漏 tick 的正常边界清零“连续超期”计数；不影响其他故障。 */
void SafetyGuard_ReportOnTimeTick(void);

/* 用途：统一停车，清 PWM/方向脚/控制积分并锁存原因；禁止 ISR 调用。 */
void SafetyGuard_Stop(StopReason reason, FaultCode fault, uint32_t tick);

/* 用途：在上层确认条件恢复后清锁存；不启动电机。 */
void SafetyGuard_ClearFault(uint32_t tick);

/* 用途：返回安全状态快照；无副作用。 */
SafetyStatus SafetyGuard_GetStatus(void);

#endif /* SAFETY_GUARD_H */
// ----- AI
