# 实施核查和必要细化

本文记录 `1-3` 落地时发现的实物/协议细节，不改变总体架构。

1. `PA25` 在队友实际 SysConfig 中已连接 `LINE_D6`，因此不能作为后加电池 ADC。电池采样仍为后加项，届时必须重新从 PCB 查找可用 ADC 通道。
2. 队友工程绑定旧电脑 SDK 2.10 和工作区路径，已改为本机 MSPM0 SDK 2.11.00.07；启动文件从误留的 `.bak` 恢复为在用 `.c`。
3. 为完成批准方案的电机辨识和速度 PI 阶段，`START_TRIAL` 明确增加 LINE_FOLLOW、WHEEL_SPEED、OPEN_LOOP_PWM 三种受限模式。非循线模式不执行灰度丢线判定，但仍执行 5 秒、通信、堵转和超期保护。
4. UART FIFO 的 RX 阈值设为 1 字节，避免短帧尾部留在半满阈值以下。
5. MPU6050 和旧单环 PID 原样归档为 `.disabled`，不进入首版构建和启动路径。
6. TI 官方启动文件与 SysConfig 自动生成文件不插入 AI 标记、不翻译供应商注释；所有人工维护 C/Python 模块均使用中文模块说明和 `----- AI` 分隔。
7. Flash 两槽使用 TI Clang `location+noinit+retain` 在 `0x1F800`/`0x1FC00` 各保留 1 KiB，构建脚本检查 map，防止与程序段重叠。
