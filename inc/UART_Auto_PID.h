/* UART2 自动调参协议、控制快照与主循环轮询服务。 */

#ifndef UART_AUTO_PID_H
#define UART_AUTO_PID_H

#include <stdbool.h>
#include <stdint.h>

#include "control_types.h"

#define UART_AUTO_PID_MAX_LINE (255U)
#define UART_AUTO_PID_TELEMETRY_COLUMNS (21U)

typedef void (*UARTAutoPidResetCallback)(void *context);

typedef struct {
    /* 指向运行时参数表；仅完整且有效的 SETALL 才会原子写入。 */
    ControlParams *params;
    /* 参数提交后请求调用者清空两个 PI 积分和循线历史。 */
    UARTAutoPidResetCallback requestControlReset;
    void *callbackContext;
} UARTAutoPidConfig;

/*
 * 控制周期写入的轻量快照。生产者只复制整数，不格式化、不等待、
 * 不读写 UART；实际 DriverLib I/O 全部由主循环中的 Service 完成。
 */
typedef struct {
    uint32_t timestampMs;
    int16_t lineError;
    int16_t leftTarget;
    int16_t leftSpeed;
    int16_t rightTarget;
    int16_t rightSpeed;
    int16_t leftPwm;
    int16_t rightPwm;
    bool running;
} UARTAutoPidControlSample;

/*
 * 在 SYSCFG_DL_init 和控制参数初始化完成后，由 main 上下文调用一次。
 * 清空行接收器、遥测节流状态和软件 TX FIFO；不执行硬件收发。
 */
void UART_Auto_PID_Init(const UARTAutoPidConfig *config);

/*
 * 仅在 main 的 while 循环轮询：读尽 UART2 当前已有的 RX 字节，并且只在
 * 硬件 TX FIFO 尚有空间时发送软件 FIFO 字节。该调用从不等待字节到达或
 * 等待发送完成。CR 被忽略，LF 结束命令，超过 127 字节的整行丢弃到下一个
 * LF。512 字节软件 TX FIFO 只按完整帧入队，空间不足时整帧不入队；绝不在
 * ISR 或 LineRun_ControlTick 调用本服务。
 */
void UART_Auto_PID_Service(void);

/*
 * 无硬件测试入口：处理一条 ASCII 命令并返回受保护 SETALL 或 RUN 是否
 * 已被接受。STATUS 不改变参数/运行状态，且此入口不排队响应或执行 UART I/O。
 */
bool UART_Auto_PID_ProcessLineForTest(const char *line);

/*
 * 由 LineRun_ControlTick 在 Motor_SetSpeed 后提供快照；每三个控制 tick 仅
 * 锁存一次待发送遥测。这里没有格式化、FIFO 访问或 DriverLib 调用。
 */
void UART_Auto_PID_OnControlSample(const UARTAutoPidControlSample *sample);

/* 由 LineRun_Stop 在主循环上下文通知本轮结束，排队一帧 running=0 遥测。 */
void UART_Auto_PID_OnRunStopped(void);

/* 仅供纯逻辑契约测试观察三分频脉冲；不消费待发送遥测。 */
bool UART_Auto_PID_IsTelemetryReadyForTest(void);

#endif /* UART_AUTO_PID_H */
