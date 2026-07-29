# 迁入 Clear_Car_Hand_Tracking 稳定参数

## 目标

将 `Clear_Car_Hand_Tracking/src/control_params.c` 已实车验证的固定运行参数写入 `Legacy/Clean_Car/src/control_params.c`，使 Legacy 工程启动默认使用同一组参数。

## 修改范围

- 只替换 `gControlParams` 的 15 个实际参数值：左右速度 PI、两侧前馈、循线 PD、滤波/积分限制、速度目标限制、PWM 限制和导数限制。
- 移除六个 `CLEAN_CAR_EXPORTED_*` 旧自动整定宏；它们仅代表被替换的旧默认值。
- 保留 Legacy 的 `ControlParams_GetMutableForUart()`、UART 协议、参数校验结构及其较宽的写入范围，因此手动上位机的六参数 SETALL 仍可用。
- 更新或替换仅断言旧自动整定默认值的校验脚本，使其验证新的稳定默认值。

## 不修改

- 不修改 `UART_Auto_PID.c`、`line_run.c`、电机、编码器、SysConfig 或 Flash 行为。
- 不烧录、不连接串口或运行电机。

## 验证

- 运行与参数文件相关的 PowerShell 静态校验。
- 编译 Legacy/Clean_Car 的现有 Debug 目标；编译通过只证明源码和链接有效，不证明实车效果。
