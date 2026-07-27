/* UART 自动调参协议的无硬件核心。 */

#ifndef UART_AUTO_PID_H
#define UART_AUTO_PID_H

#include <stdbool.h>
#include <stdint.h>

#include "control_types.h"

#define UART_AUTO_PID_MAX_LINE (127U)

typedef void (*UARTAutoPidResetCallback)(void *context);

typedef struct {
    /* 指向运行时参数表；仅完整且有效的 SETALL 才会写入。 */
    ControlParams *params;
    /* 参数提交后请求调用者清空两个 PI 积分和循线历史。 */
    UARTAutoPidResetCallback requestControlReset;
    void *callbackContext;
} UARTAutoPidConfig;

/* 供后续 UART 遥测格式化保存的控制快照；本阶段不执行任何 UART I/O。 */
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

/* 初始化协议状态及其可注入的参数/复位接口；无硬件副作用。 */
void UART_Auto_PID_Init(const UARTAutoPidConfig *config);

/*
 * 处理一条已完整接收的 ASCII 命令，允许末尾 CR。
 * 返回 true 仅表示一个完整、校验通过并已提交的 SETALL；STATUS 不改变参数。
 */
bool UART_Auto_PID_ProcessLineForTest(const char *line);

/* 保存一次控制快照，并且仅每第三个快照置位遥测就绪标志；无 UART I/O。 */
void UART_Auto_PID_OnControlSample(const UARTAutoPidControlSample *sample);

/* 仅供主机测试检查遥测三分频状态；不消费该状态。 */
bool UART_Auto_PID_IsTelemetryReadyForTest(void);

#endif /* UART_AUTO_PID_H */
