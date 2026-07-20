// ----- AI
/*
 * 协议基础模块的纯 C 测试契约。
 *
 * 该文件不访问 MSPM0 寄存器，用于检查 CRC、环形缓冲、控制数学、安全计时和统计边界。
 * 本机尚无可执行 ARM 代码的模拟器，因此 CI 至少将它严格交叉编译；
 * 返回值检查在硬件单元测试固件中可直接复用。
 */

#include <stdbool.h>
#include <stdint.h>

#include "autotune_types.h"
#include "crc16.h"
#include "line_control.h"
#include "ring_buffer.h"
#include "safety_guard.h"
#include "speed_pi.h"
#include "trial_stats.h"

static uint16_t gMotorStopCalls;
static uint16_t gControlResetCalls;
static volatile int gTestResult;

/* 仅供离线契约 ELF 链接占位，不是可烧录的 MSPM0 启动向量表。 */
void (*const interruptVectors[])(void)
    __attribute__((used, section(".intvecs"))) = {0};

int main(void);

/*
 * 用途：满足 TI 链接器默认入口并保存 main 返回码。
 * 输入/输出：无参数、无返回；结果写入 gTestResult。前置条件：仅用于离线契约 ELF。
 * 副作用：完成后永久停留；不做真实复位初始化，禁止烧录，也禁止 ISR 调用。
 */
void _c_int00(void)
{
    gTestResult = main();
    for (;;) {
        /* 保存测试返回码后不继续执行未知内存。 */
    }
}

/*
 * 用途：替代安全模块依赖的真实统一停车函数。
 * 输入/输出：无参数、无返回；前置条件：只允许契约测试链接。
 * 副作用：递增停车调用计数，不访问寄存器；禁止 ISR 调用。
 */
void Motor_Stop(void)
{
    gMotorStopCalls++;
}

static void Test_ResetControl(void)
{
    gControlResetCalls++;
}

static int Test_CRC(void)
{
    static const uint8_t standardVector[] = "123456789";
    static const uint8_t frameBody[] = {
        0x01U, 0x01U, 0x01U, 0x34U, 0x12U, 0x03U, 0x00U, 0x61U, 0x62U, 0x63U
    };

    if (CRC16_Calculate(standardVector, 9U) != 0x29B1U) {
        return 1;
    }
    if (CRC16_Calculate(frameBody, sizeof(frameBody)) != 0x5807U) {
        return 2;
    }
    return 0;
}

static int Test_RingBuffer(void)
{
    uint8_t storage[4];
    uint8_t value = 0U;
    RingBuffer buffer;

    RingBuffer_Init(&buffer, storage, sizeof(storage));
    if (!RingBuffer_IsEmpty(&buffer) || RingBuffer_Count(&buffer) != 0U) {
        return 10;
    }
    if (!RingBuffer_PushFromISR(&buffer, 1U) ||
        !RingBuffer_PushFromISR(&buffer, 2U) ||
        !RingBuffer_PushFromISR(&buffer, 3U)) {
        return 11;
    }
    if (RingBuffer_PushFromISR(&buffer, 4U)) {
        return 12;
    }
    if (!RingBuffer_Pop(&buffer, &value) || value != 1U) {
        return 13;
    }
    if (!RingBuffer_PushFromISR(&buffer, 4U)) {
        return 14;
    }
    if (RingBuffer_Count(&buffer) != 3U) {
        return 15;
    }
    if (!RingBuffer_Pop(&buffer, &value) || value != 2U ||
        !RingBuffer_Pop(&buffer, &value) || value != 3U ||
        !RingBuffer_Pop(&buffer, &value) || value != 4U) {
        return 16;
    }
    if (RingBuffer_Pop(&buffer, &value) || !RingBuffer_IsEmpty(&buffer)) {
        return 17;
    }
    return 0;
}

static int Test_SpeedPI(void)
{
    SpeedPIState state = {0};
    SpeedPIOutput output = SpeedPI_Step(
        &state, 10, 5, Q16_ONE, 0, 0, 100, 100);

    if ((output.pwm != 5) || output.saturated || (state.integral != 5)) {
        return 20;
    }

    SpeedPI_Reset(&state);
    output = SpeedPI_Step(
        &state, 10, 0, 100L * Q16_ONE, 0, 0, 100, 100);
    if ((output.pwm != 100) || !output.saturated || (state.integral != 0)) {
        return 21;
    }

    state.integral = 25;
    output = SpeedPI_Step(
        &state, 0, 5, Q16_ONE, Q16_ONE, 0, 100, 100);
    if ((output.pwm != 0) || output.saturated || (state.integral != 0)) {
        return 22;
    }
    return 0;
}

static int Test_LineControl(void)
{
    LineControlState state = {0, 0, false};
    LineSensorSample sample = {0x18U, 2U, true, false, 100};
    ControlParams params = {0};
    LineControlOutput output;

    params.lineKpQ16 = Q16_ONE;
    params.derivativeAlphaQ16 = 0;
    params.baseSpeed = 20;
    params.maxTargetSpeed = 25;
    params.maxDeltaSpeed = 10;
    params.derivativeLimit = 1000;
    output = LineControl_Step(&state, &sample, &params);
    if ((output.deltaSpeed != 10) || (output.leftTarget != 10) ||
        (output.rightTarget != 25) || !output.targetSaturated) {
        return 30;
    }

    params.lineKpQ16 = 0;
    params.lineKdQ16 = Q16_ONE;
    params.derivativeAlphaQ16 = Q16_ONE / 2;
    params.baseSpeed = 0;
    params.maxTargetSpeed = 100;
    params.maxDeltaSpeed = 100;
    params.derivativeLimit = 40;
    sample.error = 200;
    output = LineControl_Step(&state, &sample, &params);
    if ((state.filteredDerivative != 20) || (output.deltaSpeed != 20) ||
        (output.leftTarget != -20) || (output.rightTarget != 20) ||
        output.targetSaturated) {
        return 31;
    }

    sample.valid = false;
    output = LineControl_Step(&state, &sample, &params);
    if ((output.leftTarget != 0) || (output.rightTarget != 0) ||
        (output.deltaSpeed != 0)) {
        return 32;
    }
    return 0;
}

static int Test_SafetyGuard(void)
{
    ControlParams params = {0};
    ControlSample sample = {0};
    SafetyStatus status;
    uint16_t tick;

    params.stallPwmThreshold = 100;
    params.stallSpeedThreshold = 2;
    params.controlOverrunLimit = 3U;
    sample.line.valid = true;

    gMotorStopCalls = 0U;
    gControlResetCalls = 0U;
    SafetyGuard_Init(Test_ResetControl, 0U);
    SafetyGuard_BeginTrial(0U);
    if (SafetyGuard_Evaluate(39U, &sample, &params, true) ||
        !SafetyGuard_Evaluate(40U, &sample, &params, true)) {
        return 40;
    }
    status = SafetyGuard_GetStatus();
    if ((status.fault != FAULT_COMM_TIMEOUT) || (gMotorStopCalls != 2U) ||
        (gControlResetCalls != 2U)) {
        return 41;
    }

    SafetyGuard_Init(Test_ResetControl, 0U);
    SafetyGuard_BeginTrial(0U);
    sample.line.valid = false;
    for (tick = 1U; tick < SAFETY_LINE_LOST_TICKS; tick++) {
        if (SafetyGuard_Evaluate(tick, &sample, &params, true)) {
            return 42;
        }
    }
    if (!SafetyGuard_Evaluate(
            SAFETY_LINE_LOST_TICKS, &sample, &params, true) ||
        (SafetyGuard_GetStatus().fault != FAULT_LINE_LOST)) {
        return 43;
    }

    SafetyGuard_Init(Test_ResetControl, 0U);
    SafetyGuard_BeginTrial(0U);
    sample.line.valid = true;
    sample.leftPwm = 101;
    sample.leftSpeed = 0;
    for (tick = 1U; tick < SAFETY_STALL_TICKS; tick++) {
        if (SafetyGuard_Evaluate(tick, &sample, &params, true)) {
            return 44;
        }
    }
    if (!SafetyGuard_Evaluate(SAFETY_STALL_TICKS, &sample, &params, true) ||
        (SafetyGuard_GetStatus().fault != FAULT_STALL_LEFT)) {
        return 45;
    }

    SafetyGuard_Init(Test_ResetControl, 0U);
    SafetyGuard_BeginTrial(0U);
    if (SafetyGuard_ReportOverrun(1U, 2U, &params) ||
        !SafetyGuard_ReportOverrun(2U, 1U, &params) ||
        (SafetyGuard_GetStatus().fault != FAULT_CONTROL_OVERRUN)) {
        return 46;
    }
    return 0;
}

static int Test_TrialStats(void)
{
    ControlSample samples[3] = {0};
    TrialSummary summary;

    samples[0].line.valid = true;
    samples[0].line.error = 1000;
    samples[0].leftTarget = 10;
    samples[0].rightTarget = 10;
    samples[0].leftSpeed = 8;
    samples[0].rightSpeed = 9;
    samples[0].leftPwm = 50;
    samples[0].rightPwm = 60;
    samples[1] = samples[0];
    samples[1].line.valid = false;
    samples[1].line.error = -1000;
    samples[1].line.specialPattern = true;
    samples[1].targetSaturated = true;
    samples[1].pwmSaturated = true;
    samples[2] = samples[0];
    samples[2].line.error = 0;

    TrialStats_Reset(7U, 100, 200);
    TrialStats_AddSample(&samples[0]);
    TrialStats_AddSample(&samples[1]);
    TrialStats_AddSample(&samples[2]);
    TrialStats_AddOverruns(2U);
    summary = TrialStats_Finish(
        STOP_REASON_TRIAL_COMPLETE, FAULT_NONE, 130, 240);

    if ((summary.sampleCount != 3U) || (summary.paramVersion != 7U) ||
        (summary.absErrorSum != 2000) ||
        (summary.squaredErrorSum != 2000000) ||
        (summary.maxAbsError != 1000) ||
        (summary.approximateP95Error != 1500) ||
        (summary.lostSamples != 1U) || (summary.longestLostTicks != 1U) ||
        (summary.signFlips != 1U) ||
        (summary.targetSaturationSamples != 1U) ||
        (summary.pwmSaturationSamples != 1U) ||
        (summary.specialPatternSamples != 1U) ||
        (summary.controlOverruns != 2U) ||
        (summary.leftEncoderCounts != 30) ||
        (summary.rightEncoderCounts != 40)) {
        return 50;
    }
    return 0;
}

int main(void)
{
    int result = Test_CRC();

    if (result != 0) {
        return result;
    }
    result = Test_RingBuffer();
    if (result != 0) {
        return result;
    }
    result = Test_SpeedPI();
    if (result != 0) {
        return result;
    }
    result = Test_LineControl();
    if (result != 0) {
        return result;
    }
    result = Test_SafetyGuard();
    if (result != 0) {
        return result;
    }
    return Test_TrialStats();
}
// ----- AI
