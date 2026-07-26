// ----- AI
/*
 * 单个电机的速度 PI 控制。
 *
 * 本文件每 6.667 ms 被调用一次：输入“想要的轮速”和“编码器测到的轮速”，
 * 返回本次应交给电机驱动的 PWM 值。正负号同时表示电机方向。
 * 所有乘法使用 64 位中间量，避免 Q16.16 参数相乘时溢出。
 */

#include <stdbool.h>
#include <stdint.h>

#include "control_types.h"
#include "speed_pi.h"

static int32_t SpeedPI_ClampI32(int32_t value, int32_t limit);
static int16_t SpeedPI_ClampI16(int64_t value, int16_t limit, bool *saturated);
static int64_t SpeedPI_CalculateRaw(int16_t target, int32_t error,
    int32_t integral, int32_t kpQ16, int32_t kiQ16, int32_t feedforwardQ16);

void SpeedPI_Reset(SpeedPIState *state)
{
    if (state != 0) {
        /* 清除历史累计误差。停车、目标速度为 0 时会调用这里。 */
        state->integral = 0;
    }
}

SpeedPIOutput SpeedPI_Step(SpeedPIState *state, int16_t target,
    int16_t actual, int32_t kpQ16, int32_t kiQ16, int32_t feedforwardQ16,
    int32_t integralLimit, int16_t pwmLimit)
{
    /*
     * result 是本函数最终返回给调用者的数据：
     * - result.pwm：本轮电机 PWM 命令；0 为停止，正/负表示相反转向。
     * - result.saturated：true 表示原本算出的 PWM 太大，已被限制到 pwmLimit。
     */
    SpeedPIOutput result = {0, false};
    /* error：本轮“还差多少速度”＝目标速度 - 实际速度。 */
    int32_t error;
    /* candidateIntegral：若本次允许累计后，新的历史误差总和。 */
    int32_t candidateIntegral;
    /* candidateRaw：尚未限制幅度的 PWM 计算结果。 */
    int64_t candidateRaw;
    bool candidateSaturated = false;
    /* candidatePwm：限制到 pwmLimit 后，实际可返回的 PWM。 */
    int16_t candidatePwm;

    /*
     * 无效状态、目标为 0 或 PWM 上限无效时：
     * 不计算、不驱动电机，返回 { pwm = 0, saturated = false }。
     */
    if ((state == 0) || (target == 0) || (pwmLimit <= 0)) {
        SpeedPI_Reset(state);
        return result;
    }

    /* ==================== 一次 PI 计算（输入到 PWM） ====================
     * 1. 计算当前速度差 error。
     * 2. 将该差值累积为 I 项，但不超过 integralLimit。
     * 3. 用“前馈 + P + I”算出原始 PWM。
     * 4. 将 PWM 限制在 -pwmLimit 到 +pwmLimit，防止输出过大。
     *
     * 调参时不需要理解 Q16.16 的内部换算；kpQ16、kiQ16、
     * feedforwardQ16 是保存参数的内部格式。
     */
    error = (int32_t) target - actual;
    candidateIntegral = SpeedPI_ClampI32(
        state->integral + error, integralLimit);
    candidateRaw = SpeedPI_CalculateRaw(target, error, candidateIntegral,
        kpQ16, kiQ16, feedforwardQ16);
    candidatePwm = SpeedPI_ClampI16(
        candidateRaw, pwmLimit, &candidateSaturated);

    /*
     * 只有未顶到 PWM 上限，或误差正在把输出拉回可用范围时，才保存 I 项。
     * 这是防止积分一直累加、导致松开限制后突然猛冲的保护。
     */
    if (!candidateSaturated ||
        ((candidatePwm >= pwmLimit) && (error < 0)) ||
        ((candidatePwm <= -pwmLimit) && (error > 0))) {
        state->integral = candidateIntegral;
    } else {
        /* PWM 已顶住且误差仍在同方向增大：丢弃本次 I 项，只重新算输出。 */
        candidateRaw = SpeedPI_CalculateRaw(target, error, state->integral,
            kpQ16, kiQ16, feedforwardQ16);
        candidatePwm = SpeedPI_ClampI16(
            candidateRaw, pwmLimit, &candidateSaturated);
    }

    /* 将本次真正可用的电机命令和“是否被限幅”一起返回。 */
    result.pwm = candidatePwm;
    result.saturated = candidateSaturated;
    return result;
}

static int32_t SpeedPI_ClampI32(int32_t value, int32_t limit)
{
    /* 把积分累计值限制在 ±limit；limit 无效时禁用积分，返回 0。 */
    if (limit <= 0) {
        return 0;
    }
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

static int16_t SpeedPI_ClampI16(int64_t value, int16_t limit, bool *saturated)
{
    /* 把原始 PWM 限制在 ±limit，并通过 saturated 告知调用者是否发生限幅。 */
    if (value > limit) {
        *saturated = true;
        return limit;
    }
    if (value < -limit) {
        *saturated = true;
        return (int16_t) -limit;
    }
    *saturated = false;
    return (int16_t) value;
}

static int64_t SpeedPI_CalculateRaw(int16_t target, int32_t error,
    int32_t integral, int32_t kpQ16, int32_t kiQ16, int32_t feedforwardQ16)
{
    /*
     * PWM 原始值 = 前馈×目标速度 + P×当前速度差 + I×累计速度差。
     * 返回值尚未受 pwmLimit 限制；具体系数如何换算由 Q16_ONE 处理，
     * 普通调参只需关注外部显示的前馈、Kp、Ki 数值。
     */
    int64_t q16Sum = ((int64_t) feedforwardQ16 * target) +
        ((int64_t) kpQ16 * error) + ((int64_t) kiQ16 * integral);
    return q16Sum / Q16_ONE;
}
// ----- AI
