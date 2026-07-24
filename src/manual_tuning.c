/*
 * 手动烧录调参表。
 * 修改此文件的数值后，必须重新编译、烧录并完成一次安全试验。
 * 增益使用 Q16.16：1.0 = Q16_ONE；速度单位为每 10 ms 编码器计数。
 */

#include "autotune_types.h"
#include "manual_tuning.h"

static const ControlParams gManualParams = {
    /* 左速度环：Kp、Ki、前馈 */
    8L * Q16_ONE, 0, 28L * Q16_ONE,
    /* 右速度环：Kp、Ki、前馈 */
    8L * Q16_ONE, 0, 23L * Q16_ONE,
    /* 位置比例修正；Kd 先以小值抑制快速偏移，避免过度摆动。 */
    (Q16_ONE / 128), (Q16_ONE / 1024),
    /* D 项滤波、速度环积分限幅 */
    (Q16_ONE / 2), 10000,
    /* 基础速度、单轮目标上限、左右最大差速 */
    10, 100, 100,
    /* PWM 上限、D 项原始变化限幅 */
    200, 3500,
    /* 堵转 PWM 阈值、堵转速度阈值 */
    80, 1,
    /* 保留的遥测频率、连续控制超期上限 */
    10U, 3U
};

const ControlParams *ManualTuning_GetParams(void)
{
    return &gManualParams;
}
