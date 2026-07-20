# basic-1 二进制协议

## 帧

所有多字节整数均为小端：

| 字段 | 长度 |
| --- | ---: |
| Magic `A5 5A` | 2 |
| Protocol version（1） | 1 |
| Message type | 1 |
| Flags | 1 |
| Sequence | 2 |
| Payload length（最大 128） | 2 |
| Payload | N |
| CRC-16/CCITT-FALSE | 2 |

CRC 覆盖 Version 至 Payload，参数为 `poly=0x1021, init=0xFFFF, xorout=0`。改变状态的命令使用 `Session ID + Sequence` 去重；重复命令返回缓存响应，不重复启动试验或写 Flash。

## 命令

| Type | 名称 | Payload |
| ---: | --- | --- |
| `01` | HELLO | 空 |
| `02` | HEARTBEAT | `session:u32` |
| `03` | SET_PARAMS | `session:u32 + ControlParams:58B` |
| `04` | GET_STATUS | `session:u32` |
| `05` | ARM | `session:u32 + paramVersion:u16` |
| `06` | START_TRIAL | `session:u32 + mode:u8 + left:i16 + right:i16` |
| `07` | ABORT_TRIAL | `session:u32` |
| `08` | CLEAR_FAULT | `session:u32` |
| `09` | SET_TELEMETRY | `session:u32 + enabled:u8 + hz:u8` |
| `0A` | COMMIT_PARAMS | `session:u32 + paramVersion:u16` |

`START_TRIAL` 模式：

- `0 LINE_FOLLOW`：left/right 忽略；执行循线 PD + 左右速度 PI，启用丢线联锁。
- `1 WHEEL_SPEED`：left/right 是每 10 ms 编码器目标计数；用于车轮悬空速度阶跃，不使用灰度丢线判定。
- `2 OPEN_LOOP_PWM`：left/right 是有符号 PWM；用于车轮悬空电机辨识，不使用灰度丢线判定。

三种模式均保持 500 tick 上限、400 ms 通信超时、300 ms 堵转、控制超期和统一停车。命令值还必须通过当前参数上限，首版 PWM 无线硬上限为 400。

## 响应和事件

| Type | 名称 | 说明 |
| ---: | --- | --- |
| `80` | ACK | 普通 ACK 为 `command:u8,status:u8,paramVersion:u16,session:u32`；HELLO ACK 为 16 B 能力结构 |
| `81` | NACK | 与普通 ACK 同布局，status 为拒绝原因 |
| `82` | STATUS | 状态、故障、停止原因、版本、trial tick、control tick、overrun、session |
| `83` | TELEMETRY | 24 B；tick、位图、误差、目标/实际速度、PWM、状态、故障、版本 |
| `84` | TRIAL_SUMMARY | 128 B；本地最终评分统计 |
| `85` | EVENT | 预留事件类型 |

HELLO ACK：`command,status,paramVersion,session,protocol,fwMajor,fwMinor,fwPatch,capabilities:u32`。

`TRIAL_SUMMARY` 包含：样本数、运行 tick、参数版本、停止/故障、IAE、误差平方和、最大误差、P95 直方图估计、丢线、目标/PWM 饱和、符号翻转、控制超期、左右编码器增量、目标/速度/PWM 累加以及速度绝对/平方误差和。实时遥测是否丢失不影响该汇总。

## 时间和可靠性

- Python 每 100 ms 发 HEARTBEAT。
- MCU 只用完整、长度合法且 CRC 正确的帧刷新心跳。
- 9600 baud 默认遥测 10 Hz，最大 20 Hz。
- UART ISR 只搬字节；RX FIFO 阈值为 1 字节；协议解析和 CRC 在主循环。
- 高优先级 ACK/汇总与可丢遥测使用不同 TX 队列，高优先级先发送。
