// ----- AI
/* 试验统计实现；P95 用 500 误差宽度的 8 档直方图估计。 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "autotune_types.h"
#include "trial_stats.h"

#define ERROR_SIGN_DEADBAND (250)
#define HISTOGRAM_WIDTH     (500)

static TrialSummary gSummary;
static uint16_t gHistogram[TRIAL_HISTOGRAM_BINS];
static uint16_t gCurrentLostTicks;
static int8_t gLastErrorSign;
static int32_t gStartLeftCount;
static int32_t gStartRightCount;

static int16_t TrialStats_AbsI16(int16_t value);
static int64_t TrialStats_AddSatI64(int64_t current, int64_t value);
static uint16_t TrialStats_AddSatU16(uint16_t current, uint16_t value);

void TrialStats_Reset(
    uint16_t paramVersion, int32_t leftCount, int32_t rightCount)
{
    (void) memset(&gSummary, 0, sizeof(gSummary));
    (void) memset(gHistogram, 0, sizeof(gHistogram));
    gSummary.paramVersion = paramVersion;
    gCurrentLostTicks = 0U;
    gLastErrorSign = 0;
    gStartLeftCount = leftCount;
    gStartRightCount = rightCount;
}

void TrialStats_AddSample(const ControlSample *sample)
{
    int16_t absError;
    uint16_t histogramIndex;
    int8_t sign = 0;

    if ((sample == 0) || (gSummary.sampleCount == UINT16_MAX)) {
        return;
    }
    gSummary.sampleCount++;
    gSummary.runTicks++;
    absError = TrialStats_AbsI16(sample->line.error);
    gSummary.absErrorSum = TrialStats_AddSatI64(gSummary.absErrorSum, absError);
    gSummary.squaredErrorSum = TrialStats_AddSatI64(gSummary.squaredErrorSum,
        (int64_t) sample->line.error * sample->line.error);
    if (absError > gSummary.maxAbsError) {
        gSummary.maxAbsError = absError;
    }
    histogramIndex = (uint16_t) absError / HISTOGRAM_WIDTH;
    if (histogramIndex >= TRIAL_HISTOGRAM_BINS) {
        histogramIndex = TRIAL_HISTOGRAM_BINS - 1U;
    }
    gHistogram[histogramIndex] =
        TrialStats_AddSatU16(gHistogram[histogramIndex], 1U);

    if (!sample->line.valid) {
        gSummary.lostSamples = TrialStats_AddSatU16(gSummary.lostSamples, 1U);
        gCurrentLostTicks = TrialStats_AddSatU16(gCurrentLostTicks, 1U);
        if (gCurrentLostTicks > gSummary.longestLostTicks) {
            gSummary.longestLostTicks = gCurrentLostTicks;
        }
    } else {
        gCurrentLostTicks = 0U;
    }
    if (sample->line.specialPattern) {
        gSummary.specialPatternSamples =
            TrialStats_AddSatU16(gSummary.specialPatternSamples, 1U);
    }

    gSummary.leftTargetSum = TrialStats_AddSatI64(
        gSummary.leftTargetSum, sample->leftTarget);
    gSummary.rightTargetSum = TrialStats_AddSatI64(
        gSummary.rightTargetSum, sample->rightTarget);
    gSummary.leftSpeedSum = TrialStats_AddSatI64(
        gSummary.leftSpeedSum, sample->leftSpeed);
    gSummary.rightSpeedSum = TrialStats_AddSatI64(
        gSummary.rightSpeedSum, sample->rightSpeed);
    gSummary.leftAbsPwmSum = TrialStats_AddSatI64(
        gSummary.leftAbsPwmSum, TrialStats_AbsI16(sample->leftPwm));
    gSummary.rightAbsPwmSum = TrialStats_AddSatI64(
        gSummary.rightAbsPwmSum, TrialStats_AbsI16(sample->rightPwm));
    {
        int32_t leftSpeedError =
            (int32_t) sample->leftTarget - sample->leftSpeed;
        int32_t rightSpeedError =
            (int32_t) sample->rightTarget - sample->rightSpeed;
        int64_t absoluteError =
            ((leftSpeedError < 0) ? -leftSpeedError : leftSpeedError) +
            ((rightSpeedError < 0) ? -rightSpeedError : rightSpeedError);
        int64_t squaredError = ((int64_t) leftSpeedError * leftSpeedError) +
            ((int64_t) rightSpeedError * rightSpeedError);
        gSummary.speedAbsErrorSum = TrialStats_AddSatI64(
            gSummary.speedAbsErrorSum, absoluteError);
        gSummary.speedSquaredErrorSum = TrialStats_AddSatI64(
            gSummary.speedSquaredErrorSum, squaredError);
    }
    if (TrialStats_AbsI16(sample->leftPwm) > gSummary.maxAbsLeftPwm) {
        gSummary.maxAbsLeftPwm = TrialStats_AbsI16(sample->leftPwm);
    }
    if (TrialStats_AbsI16(sample->rightPwm) > gSummary.maxAbsRightPwm) {
        gSummary.maxAbsRightPwm = TrialStats_AbsI16(sample->rightPwm);
    }
    if (TrialStats_AbsI16(sample->leftTarget) > gSummary.maxAbsLeftTarget) {
        gSummary.maxAbsLeftTarget = TrialStats_AbsI16(sample->leftTarget);
    }
    if (TrialStats_AbsI16(sample->rightTarget) > gSummary.maxAbsRightTarget) {
        gSummary.maxAbsRightTarget = TrialStats_AbsI16(sample->rightTarget);
    }
    if (TrialStats_AbsI16(sample->leftSpeed) > gSummary.maxAbsLeftSpeed) {
        gSummary.maxAbsLeftSpeed = TrialStats_AbsI16(sample->leftSpeed);
    }
    if (TrialStats_AbsI16(sample->rightSpeed) > gSummary.maxAbsRightSpeed) {
        gSummary.maxAbsRightSpeed = TrialStats_AbsI16(sample->rightSpeed);
    }
    if (sample->targetSaturated) {
        gSummary.targetSaturationSamples =
            TrialStats_AddSatU16(gSummary.targetSaturationSamples, 1U);
    }
    if (sample->pwmSaturated) {
        gSummary.pwmSaturationSamples =
            TrialStats_AddSatU16(gSummary.pwmSaturationSamples, 1U);
    }

    if (sample->line.error > ERROR_SIGN_DEADBAND) {
        sign = 1;
    } else if (sample->line.error < -ERROR_SIGN_DEADBAND) {
        sign = -1;
    }
    if ((sign != 0) && (gLastErrorSign != 0) && (sign != gLastErrorSign)) {
        gSummary.signFlips = TrialStats_AddSatU16(gSummary.signFlips, 1U);
    }
    if (sign != 0) {
        gLastErrorSign = sign;
    }
}

void TrialStats_AddOverruns(uint16_t missedTicks)
{
    gSummary.controlOverruns =
        TrialStats_AddSatU16(gSummary.controlOverruns, missedTicks);
}

TrialSummary TrialStats_Finish(StopReason reason, FaultCode fault,
    int32_t leftCount, int32_t rightCount)
{
    uint32_t cumulative = 0U;
    uint32_t target = ((uint32_t) gSummary.sampleCount * 95U + 99U) / 100U;
    uint16_t index;

    for (index = 0U; index < TRIAL_HISTOGRAM_BINS; index++) {
        cumulative += gHistogram[index];
        if ((target != 0U) && (cumulative >= target)) {
            gSummary.approximateP95Error = (int16_t) ((index + 1U) * HISTOGRAM_WIDTH);
            break;
        }
    }
    gSummary.leftEncoderCounts = leftCount - gStartLeftCount;
    gSummary.rightEncoderCounts = rightCount - gStartRightCount;
    gSummary.stopReason = reason;
    gSummary.fault = fault;
    return gSummary;
}

static int16_t TrialStats_AbsI16(int16_t value)
{
    return (value == INT16_MIN) ? INT16_MAX :
        ((value < 0) ? (int16_t) -value : value);
}

static int64_t TrialStats_AddSatI64(int64_t current, int64_t value)
{
    if ((value > 0) && (current > INT64_MAX - value)) {
        gSummary.arithmeticSaturated = true;
        return INT64_MAX;
    }
    if ((value < 0) && (current < INT64_MIN - value)) {
        gSummary.arithmeticSaturated = true;
        return INT64_MIN;
    }
    return current + value;
}

static uint16_t TrialStats_AddSatU16(uint16_t current, uint16_t value)
{
    return (UINT16_MAX - current < value) ? UINT16_MAX :
        (uint16_t) (current + value);
}
// ----- AI
