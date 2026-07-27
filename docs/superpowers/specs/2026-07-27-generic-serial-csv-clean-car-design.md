# Generic Serial CSV Clean Car Design

## Goal

在保持 KEY1 物理启动、150 Hz 控制周期、10 秒运行上限、丢线与堵转保护不变的前提下，为 Clean_Car 增加基于 `generic_serial_csv` 的双环调参串口扩展。电脑端通过 115200、PB15 TX / PB16 RX 的透明无线串口读取遥测并写入六个控制增益。

## Boundary

控制结构不改变：循线 PD 产生左右目标速度，左右速度 PI 加前馈产生 PWM。可写参数仅为左速度 `Kp/Ki`、右速度 `Kp/Ki`、循线 `Kp/Kd`；两路前馈、`baseSpeed`、限幅与保护参数保持固件原值。

KEY1 是唯一的电机启动/停止输入。上位机不提供 `RUN`、停止解除或其他启动命令；它只能读取状态和更新参数。

## Compatibility Approach

新增 `generic_serial_csv_clean_car` 硬件 profile，复用 generic profile 的逐行 UTF-8 ASCII、CSV 遥测、`STATUS` 和串口桥架构。原 `generic_serial_csv` 的行为与格式不改变。

自动整定分两阶段：

1. `speed`：控制器 1 映射左速度 PI，控制器 2 映射右速度 PI；循线 PD 固定。
2. `line`：控制器 1 映射循线 PD，I 固定为 0；左右速度 PI 固定。

无论当前阶段为何，每次更新都由上位机下发一条包含全部六个增益的 `SETALL`，从而避免控制器处于半更新状态。

## Wire Protocol

所有帧以 `\n` 结尾，允许接收端忽略 `\r`。扩展帧在正文后附带 `*HH`，其中 `HH` 为正文 ASCII 字节的两位十六进制 XOR 校验；校验失败、字段缺失、重复、超范围或行过长时不得改变参数。

### Host to firmware

```text
STATUS
SETALL SEQ:17 LKP:12.000000 LKI:0.000000 RKP:12.000000 RKI:0.000000 LINEP:0.014286 LINED:0.003125*HH
```

`SEQ` 为 0 至 65535 的递增序号。`SETALL` 仅在六个增益均存在、有限且固件范围检查通过时生效；成功回复 `ACK,17*HH`，拒绝回复 `ERR,17,<reason>*HH`。`STATUS` 仅返回状态，不启动电机。

### Firmware telemetry

遥测每 3 个控制 tick（50 Hz）生成一次，最长 128 个 ASCII 字节。前 11 列保留 generic CSV 的控制器 1/2 位置；扩展 profile 解释其余列。

```text
timestamp_ms,setpoint,input,pwm,error,p,i,d,p2,i2,d2,line_error,left_target,left_speed,right_target,right_speed,left_pwm,right_pwm,running,line_kp,line_kd*HH
```

在 `speed` 阶段，前五列表示双轮速度的聚合响应，`p/i/d` 是左 PI，`p2/i2/d2` 是右 PI。在 `line` 阶段，前五列表示循线误差与差速响应，`p/i/d` 是循线 `Kp/0/Kd`，控制器 2 不参与自动修改。

## Real-time and Safety Rules

- UART RX 只在主循环以非阻塞方式服务；控制 ISR 仍只递增 tick。
- 发送采用非阻塞 FIFO 服务；不在 `LineRun_ControlTick()` 内格式化、等待或发送串口数据。
- 固件先构造并验证候选参数，再一次性替换运行参数；接受新增益后复位两个速度 PI 积分状态和循线历史状态。
- 缺少有效参数快照、未收到匹配 ACK 或无线数据校验失败时，上位机不得把本轮参数标为已应用。
- 车轮悬空完成首轮串口验证；参数变大后再落地低速试验。现有 `maxPwm`、10 秒、丢线与堵转保护保持不变。

## Tests

- 上位机：profile 命令生成、校验、`SETALL` ACK/ERR 处理、两种阶段的 CSV 解析与无 ACK 拒绝。
- 固件宿主测试：完整命令更新六参数、任何非法/损坏/不完整命令不改变参数、KEY1 仍是唯一启动路径、遥测节流。
- SysConfig/源码契约：UART2 PB15/PB16、115200、运行时 UART 模块存在；不手改 SysConfig 生成文件。
