// ----- AI
/*
 * 八路灰度循线的 PD 控制。
 *
 * 本文件不直接输出 PWM，也不直接驱动电机；它根据灰度传感器给出的偏离位置，
 * 算出左右轮“应该跑多快”的目标速度。随后 speed_pi.c 再把这两个目标速度变成 PWM。
 */

#include <stdbool.h>
#include <stdint.h>

#include "autotune_types.h"
#include "line_control.h"

#define LINE_DELTA_SLEW_PER_TICK (6)

static int32_t LineControl_ClampI32(int32_t value, int32_t limit);
static int32_t LineControl_SlewDelta(int16_t previous, int32_t desired);
static int16_t LineControl_ClampTarget(
    int32_t value, int16_t limit, bool *saturated);

void LineControl_Reset(LineControlState *state)
{
    if (state != 0) {
        /* 清除上一次的位置误差和 D 项滤波历史；每次新试验开始时需要清空。 */
        state->previousError = 0;
        state->filteredDerivative = 0;
        state->previousDeltaSpeed = 0;
        state->initialized = false;
    }
}

LineControlOutput LineControl_Step(LineControlState *state,
    const LineSensorSample *sample, const ControlParams *params,
    int16_t turnDamping)
{
    /*
     * result 是本函数最终返回给调用者的数据：
     * - leftTarget/rightTarget：给左右速度 PI 的目标速度，不是 PWM。
     * - deltaSpeed：为了转向而让两侧速度产生的差值。
     * - targetSaturated：true 表示左右任一目标速度超过 maxTargetSpeed，已被限制。
     */
    LineControlOutput result = {0, 0, 0, false};
    /* rawDerivative：本次位置误差相对上一次变化了多少，即 D 项的原始数据。 */
    int32_t rawDerivative;
    /* filtered：经过低通平滑后的 D 项，避免传感器抖动直接造成左右轮猛变速。 */
    int32_t filtered;
    /* deltaQ16/delta：PD 计算出的左右轮速度差，前者是内部格式，后者是实际速度单位。 */
    int64_t deltaQ16;
    int32_t delta;

    /*
     * 状态、灰度数据、参数无效，或灰度传感器没有找到有效黑线时：
     * 不更新历史数据，直接返回 { 左目标=0，右目标=0，差速=0，未限幅 }。
     * 实际的丢线停车计时由 safety_guard 模块处理。
     */
    if ((state == 0) || (sample == 0) || (params == 0)) {
        return result;
    }
    if (!sample->valid) {
        state->previousDeltaSpeed = 0;
        return result;
    }

    /* ==================== D 项：误差变化速度，并进行平滑 ====================
     * 第一次调用没有上一次数据，D 项从 0 开始；之后使用“本次误差 - 上次误差”。
     * 先限制突变幅度，再用 derivativeAlphaQ16 做低通平滑。
     * 调参时不需要理解内部 Q16.16 换算；这里对应的外部参数是 D（lineKd）。
     */
    rawDerivative = state->initialized ?
        ((int32_t) sample->error - state->previousError) : 0;
    rawDerivative = LineControl_ClampI32(
        rawDerivative, params->derivativeLimit);
    filtered = (int32_t) ((((int64_t) params->derivativeAlphaQ16 *
        state->filteredDerivative) +
        ((int64_t) (Q16_ONE - params->derivativeAlphaQ16) * rawDerivative)) /
        Q16_ONE);
    state->filteredDerivative = filtered;
    state->previousError = sample->error;
    state->initialized = true;

    /* ==================== PD 到左右目标速度 ====================
     * 1. P 项：根据当前偏线程度修正。
     * 2. D 项：根据偏线变化趋势提前修正，减少来回摆动。
     * 3. 得到 delta（左右速度差），并限制其最大值。
     * 4. 用基础速度 baseSpeed 分别减/加 delta，得到左右轮目标速度。
     *
     * delta 为正：左轮目标变快、右轮目标变慢，使车向右修正；delta 为负则相反。
     * lineKpQ16、lineKdQ16 是参数的内部格式；普通调参看 lineKp、lineKd 即可。
     */
    deltaQ16 = ((int64_t) params->lineKpQ16 * sample->error) +
        ((int64_t) params->lineKdQ16 * filtered);
    delta = (int32_t) (deltaQ16 / Q16_ONE);
    delta -= turnDamping;
    delta = LineControl_ClampI32(delta, params->maxDeltaSpeed);
    delta = LineControl_SlewDelta(state->previousDeltaSpeed, delta);
    state->previousDeltaSpeed = (int16_t) delta;
    /* 保存给调用者查看的左右速度差。 */
    result.deltaSpeed = (int16_t) delta;
    result.leftTarget = LineControl_ClampTarget(
        (int32_t) params->baseSpeed + delta,
        params->maxTargetSpeed, &result.targetSaturated);
    result.rightTarget = LineControl_ClampTarget(
        (int32_t) params->baseSpeed - delta,
        params->maxTargetSpeed, &result.targetSaturated);
    /* 返回左右速度 PI 下一步要使用的目标速度，以及是否有目标速度被限幅。 */
    return result;
}

static int32_t LineControl_ClampI32(int32_t value, int32_t limit)
{
    /* 把数值限制在 ±limit；用于限制 D 项突变和左右最大差速。 */
    if (limit < 0) {
        limit = -limit;
    }
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

static int32_t LineControl_SlewDelta(int16_t previous, int32_t desired)
{
    int32_t change = desired - previous;

    if (change > LINE_DELTA_SLEW_PER_TICK) {
        return (int32_t) previous + LINE_DELTA_SLEW_PER_TICK;
    }
    if (change < -LINE_DELTA_SLEW_PER_TICK) {
        return (int32_t) previous - LINE_DELTA_SLEW_PER_TICK;
    }
    return desired;
}

static int16_t LineControl_ClampTarget(
    int32_t value, int16_t limit, bool *saturated)
{
    /* 把单侧目标速度限制在 ±maxTargetSpeed；超出时记录 targetSaturated=true。 */
    if (value > limit) {
        *saturated = true;
        return limit;
    }
    if (value < -limit) {
        *saturated = true;
        return (int16_t) -limit;
    }
    return (int16_t) value;
}
// ----- AI
