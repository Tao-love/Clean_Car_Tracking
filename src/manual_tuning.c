/*
 * 手动烧录调参表。
 * 修改此文件的数值后，必须重新编译、烧录并完成一次安全试验。
 * 增益使用 Q16.16：1.0 = Q16_ONE；速度单位为每 6.667 ms 编码器计数。
 */

#include "autotune_types.h"
#include "manual_tuning.h"

static const ControlParams gManualParams = {
    /* 左速度环：Kp、Ki、前馈 */
    12L * Q16_ONE, 0, 42L * Q16_ONE,
    /* 右速度环：Kp、Ki、前馈 */
    12L * Q16_ONE, 0, (69L * Q16_ONE / 2),
    /* 提前拉回并抑制摆动：Kp 保持，Kd 在当前基础上再提高约 12%。 */
    (Q16_ONE / 70), (Q16_ONE / 320),
    /* D 项滤波、速度环积分限幅 */
    (5L * Q16_ONE / 8), 10000,
    /* 基础速度、单轮目标上限、左右最大差速 */
    27, 67, 67,
    /* PWM 上限、D 项原始变化限幅 */
    300, 2333,
    /* 堵转 PWM 阈值、堵转速度阈值 */
    80, 1,
    /* 保留的遥测频率、连续控制超期上限 */
    10U, 5U
};

const ControlParams *ManualTuning_GetParams(void)
{
    return &gManualParams;
}
