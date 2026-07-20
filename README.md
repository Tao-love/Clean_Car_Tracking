# basic-1：MSPM0G3507 循迹小车无线自动整定

`basic-1` 基于队友初版工程实现已批准的 `1-3` 架构：MSPM0G3507 在本地完成 100 Hz 双闭环、最长 5 秒试验、安全停车和统计；Python 通过 JDY-31 独占蓝牙 COM 口，下发候选参数、评分、自动搜索、记录日志，并可选把遥测转发给 VOFA+。

## 当前安全基线

- 上电默认 `base_speed=0`，不会自动启动车辆。
- 每次试验最多 `500 × 10 ms`，由 MCU 本地停车。
- RUNNING 时 400 ms 无合法帧、150 ms 丢线、300 ms 堵转或连续控制超期均由 MCU 本地停车。
- TB6612FNG 的 `STBY` 在现有 PCB 上固定 5 V；统一停车同时把 `PWMA/PWMB=0` 和 `AIN1/AIN2/BIN1/BIN2=0`。
- 无线参数硬限制 `max_pwm<=400`，即 1600 满量程的 25%；提高上限必须在悬空和低速验收后修改固件受限常量，不能只靠无线命令绕过。
- 电池 ADC、电流检测、直角专用状态机和 MPU6050 闭环不属于首版。`PA25` 已用于第 7 路灰度 `LINE_D6`，不是空闲 ADC 引脚。

## JDY-31 接线

| PCB / MSPM0 | JDY-31 |
| --- | --- |
| `PA8 / TX1` | `RXD` |
| `PA9 / RX1` | `TXD` |
| `GND` | `GND` |
| 稳定 `5V` | `VCC` |

`EN`、`STATE` 暂不接。首次联调固定 `9600 8N1`。XDS110 只用于烧录和调试，正常蓝牙调参时不需要连接。

## 目录

- `inc/`, `src/`：MSPM0 固件；UART、协议、参数、控制、安全、状态机、统计和 Flash 各自独立。
- `empty.syscfg`：PA8/PA9 UART1 9600、1 字节 RX FIFO 阈值、10 ms TIMG0、PWM、编码器和灰度配置。
- `python/autotune/`：串口、协议、会话、试验、评分、优化、日志、VOFA+ 和 CLI。
- `python/config.example.json`：不动车的保守起点；实测后复制并填写参数，不要直接把未知参数当成已标定值。
- `docs/protocol.md`：二进制协议与三种试验模式。
- `docs/hardware-test-checklist.md`：必须按顺序执行的实物验收。
- `legacy/teammate_initial/`：不参与构建的队友旧 PID/MPU6050 参考文件。

## 电脑环境

- CCS / TI Arm Clang
- MSPM0 SDK 2.11.00.07
- Python 3.13
- `pyserial==3.5`
- `numpy>=2.5,<3`
- 可选 VOFA+：本机 UDP FireWater，默认 `127.0.0.1:1347`

安装 Python 依赖：

```powershell
E:\python\python.exe -m pip install -r python\requirements.txt
```

## 构建与测试

在 CCS 中导入本目录的 `basic-1` 工程，使用 MSPM0 SDK 2.11.00.07 重新生成 SysConfig 后 Build。也可运行仓库内的严格验证脚本：

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

该脚本执行 SysConfig、全部 Python 测试、`-Wall -Wextra -Werror` 固件构建，并确认 Flash 双槽固定在 `0x1F800`/`0x1FC00`。

## 首次只测通信

电机不要落地，建议先不接电机主电源，只验证 JDY-31：

```powershell
$env:PYTHONPATH="python"
E:\python\python.exe -m autotune ports
E:\python\python.exe -m autotune --port COM7 hello
E:\python\python.exe -m autotune --port COM7 status
```

`hello` 和 `status` 不会启动电机。

## 明确启动试验的命令

所有 trial 都先执行 `SET_PARAMS → ARM → START`，并等待 MCU 的最终 `TRIAL_SUMMARY`：

```powershell
# 正常循线，启用 150 ms 丢线保护
E:\python\python.exe -m autotune --port COM7 trial --mode line

# 车轮悬空速度阶跃
E:\python\python.exe -m autotune --port COM7 trial --mode speed --left 10 --right 10

# 车轮悬空开环 PWM，仍受 max_pwm<=400 和堵转保护
E:\python\python.exe -m autotune --port COM7 trial --mode pwm --left 100 --right 100
```

自动阶段命令为 `identify`、`tune-speed`、`tune-line`、`climb`。发生安全故障后流程立即暂停，并在可能时把 RAM 参数恢复为 last-safe；不会自动清故障或重新启动。

只有在 `IDLE` 且明确确认最终参数版本后才允许：

```powershell
E:\python\python.exe -m autotune --port COM7 commit --version 12
```

## VOFA+

Python 始终独占蓝牙 COM。需要 VOFA+ 时，在配置中启用 `vofa.enabled`，然后让 VOFA+ 监听 Python 的本机 UDP FireWater 端口；不要让 VOFA+ 再打开 JDY-31 的 COM 口。VOFA+ 未运行或转发失败不会中断试验。

## 实物状态

软件构建和离线测试结果记录在版本提交中。蓝牙、电机方向、编码器方向、实际堵转阈值、低速循线、Flash 断电回退和赛道直角仍必须由使用者按 [硬件验收清单](docs/hardware-test-checklist.md) 在实物上完成，未实测项不视为已通过。
