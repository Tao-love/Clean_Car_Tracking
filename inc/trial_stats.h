// ----- AI
/*
 * 5 秒试验本地统计模块。
 * 职责：对每个 10 ms 样本累加 IAE、平方和、P95 直方图、丢线、速度、PWM、饱和与震荡。
 * 依赖：ControlSample。上下文：100 Hz 主循环任务。
 * 单位：和值保留原始误差/编码器/PWM 单位；样本数最多 500。
 * 硬件副作用：无。故障输出：64 位累加饱和时置 arithmeticSaturated。
 */

#ifndef TRIAL_STATS_H
#define TRIAL_STATS_H

#include <stdbool.h>
#include <stdint.h>

#include "autotune_types.h"

#define TRIAL_HISTOGRAM_BINS (8U)

typedef struct {
    uint16_t sampleCount;
    uint16_t runTicks;
    uint16_t paramVersion;
    uint16_t lostSamples;
    uint16_t longestLostTicks;
    uint16_t targetSaturationSamples;
    uint16_t pwmSaturationSamples;
    uint16_t signFlips;
    uint16_t controlOverruns;
    int16_t maxAbsError;
    int16_t approximateP95Error;
    int16_t maxAbsLeftPwm;
    int16_t maxAbsRightPwm;
    int32_t leftEncoderCounts;
    int32_t rightEncoderCounts;
    int64_t absErrorSum;
    int64_t squaredErrorSum;
    int64_t leftTargetSum;
    int64_t rightTargetSum;
    int64_t leftSpeedSum;
    int64_t rightSpeedSum;
    int64_t leftAbsPwmSum;
    int64_t rightAbsPwmSum;
    StopReason stopReason;
    FaultCode fault;
    bool arithmeticSaturated;
} TrialSummary;

/* 用途：清空本次统计并记录起始编码器/参数版本；禁止 ISR 调用。 */
void TrialStats_Reset(
    uint16_t paramVersion, int32_t leftCount, int32_t rightCount);

/* 用途：累加一个 10 ms 样本；只改内存，禁止 ISR 调用。 */
void TrialStats_AddSample(const ControlSample *sample);

/* 用途：记录控制漏执行 tick 数；使用饱和 16 位计数。 */
void TrialStats_AddOverruns(uint16_t missedTicks);

/* 用途：完成 P95/编码器增量/停止信息并返回稳定汇总快照。 */
TrialSummary TrialStats_Finish(StopReason reason, FaultCode fault,
    int32_t leftCount, int32_t rightCount);

#endif /* TRIAL_STATS_H */
// ----- AI
