# 延后接线的 MPU6050 重校准设计

## 目标

预置一个可供未来循线启动流程调用的 MPU6050 重校准 API，并删除上电初始化调用；本次不让 KEY1、无线 `RUN`、上电初始化或 BENCH 调用它，因此 MPU6050 不产生 I2C 配置、校准或采样，现有 PID 调参不使用陀螺仪阻尼。

## 设计

在 `inc/mpu6050.h` 声明 `void MPU6050_RecalibrateForNextRun(void);`，并在 `src/mpu6050.c` 实现该函数。该函数复用已有 `MPU6050_InitAndCalibrate()`，因而保持现有的 WHO_AM_I 检查、寄存器配置、200 次 1 ms 静止采样、读失败静默禁用和零阻尼降级行为。

本次从 `src/main.c` 删除现有 `MPU6050_InitAndCalibrate()` 上电调用，且不在 `src/line_run.c` 或 `src/UART_Auto_PID.c` 添加新 API 调用。`gImuEnabled` 因此保持默认关闭；现有 `MPU6050_Update()` 和 `MPU6050_GetTurnDamping()` 分别立即返回和返回 0，不会访问 I2C。未来获得明确授权后，只需由 `LineRun_Start()` 在置 `gRunning=true` 前调用新 API，即可同时覆盖 KEY1 和无线 `RUN`；BENCH 继续不调用。

## 验证

新增 PowerShell 契约测试。测试先要求新声明与实现存在，并明确断言 `main.c` 不再调用旧上电校准、且 `main.c`、`line_run.c` 和 `UART_Auto_PID.c` 均没有调用新 API。通过后再执行现有 `verify_line_run.ps1` 与 CCS 全量构建。构建成功不等于已在车辆上验证 MPU，且本次不烧录。
