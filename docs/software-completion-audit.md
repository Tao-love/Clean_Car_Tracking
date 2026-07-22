# 1-3 软件完成审计

审计日期：2026-07-20

审计依据：原始批准方案 `1-3.md`、仓库摘要 `docs/specs/1-3-summary.md`、当前源码、Python 测试和 `tools/verify.ps1`。本次交付边界由用户明确为：完成固件、Python、Flash、VOFA+、文档、离线测试、编译链接与 GitHub 交付；不要求烧录 MSPM0、连接 JDY-31、运行电机或进行赛道实测。

## 逐项对应

| 1-3 要求 | 实现位置 | 可离线验证证据 | 状态 |
| --- | --- | --- | --- |
| 100 Hz 单调 tick、有界追赶、超期记录 | `src/main.c`、`src/trial_manager.c` | TI Arm Clang 严格编译链接 | 软件完成 |
| UART ISR 只搬字节、非阻塞优先级队列 | `src/uart_transport.c`、`src/ring_buffer.c` | 严格编译；协议与链路模型测试 | 软件完成 |
| CRC、流式重同步、128 B 上限 | `src/crc16.c`、`src/protocol.c`、`python/autotune/protocol.py` | `python/tests/test_protocol.py` | 软件完成 |
| Session、Sequence 去重、ACK/NACK、全部首版命令 | `src/app_protocol.c`、`python/autotune/session.py`、`python/autotune/link.py` | `test_protocol.py`、`test_link.py`、`test_session.py` | 软件完成 |
| 参数范围、Q16.16、pending/active 原子切换 | `src/control_params.c`、`python/autotune/models.py` | 线格式/安全默认值模型测试；参数状态逻辑源码审计与严格编译，未运行 ARM 行为测试 | 软件完成 |
| 七态试验状态机、最多 500 tick | `src/trial_manager.c` | 上位机编排测试；MCU 状态逻辑源码审计与严格编译，未运行 ARM 行为测试 | 软件完成 |
| STBY 固定 5 V 条件下统一停车 | `src/motor.c`、`src/safety_guard.c` | 源码审计；严格编译 | 软件完成；引脚实测豁免 |
| 400 ms 通信、150 ms 丢线、300 ms 堵转、超期联锁 | `src/safety_guard.c`、`src/trial_manager.c` | 源码审计；严格编译 | 软件完成；实车触发测试豁免 |
| 左右独立速度 PI、前馈、条件积分抗饱和 | `src/speed_pi.c`、`src/trial_manager.c` | 严格编译；上位机参数模型测试 | 软件完成 |
| 八路加权中心、循线 PD、D 低通与限幅 | `src/gray_sensor.c`、`src/line_control.c` | 严格编译；权重和边界源码审计 | 软件完成 |
| LINE_FOLLOW、WHEEL_SPEED、OPEN_LOOP_PWM 三种受限试验 | `src/trial_manager.c`、`src/app_protocol.c`、`python/autotune/trial.py` | `test_trial.py`；严格编译 | 软件完成 |
| 本地统计、P95、可靠汇总、10 Hz 遥测 | `src/trial_stats.c`、`src/app_protocol.c`、`python/autotune/models.py` | `test_models.py`；严格编译 | 软件完成 |
| Python 独占 COM、心跳、有限重试、失败暂停、last-safe | `python/autotune/link.py`、`session.py`、`trial.py` | `test_link.py`、`test_session.py`、`test_trial.py` | 软件完成 |
| 可复算评分、失败惩罚、有界搜索 | `python/autotune/scoring.py`、`optimizer.py` | `test_scoring.py`、`test_optimizer.py` | 软件完成 |
| 电机辨识、速度 PI、循线 PD、速度爬升四阶段 | `python/autotune/stages.py`、`cli.py` | CLI 导入/语法检查；相关试验、评分和优化器测试 | 软件完成 |
| JSONL、原始帧、CSV 记录 | `python/autotune/logging.py`、`link.py`、`cli.py` | Python `compileall` 与试验流程测试 | 软件完成 |
| VOFA+ 可选 UDP FireWater，失败隔离 | `python/autotune/vofa.py` | `python/tests/test_vofa.py` | 软件完成 |
| Flash 双槽、generation、CRC、读回、回退 | `src/flash_params.c`、`python/autotune/flash_record.py` | `test_flash_record.py`；linker map 地址检查 | 软件完成；断电实测豁免 |
| 中文模块化注释和 AI 标记 | 人工维护的 `inc/`、`src/`、`python/`、`tools/` | 标记扫描与人工抽查；生成/供应商文件按方案例外 | 完成 |

## 验证边界

- `tools/verify.ps1` 会重新生成 SysConfig、运行全部 Python 单元测试、执行 Python `compileall`、用 TI Arm Clang 的 `-Wall -Wextra -Werror` 编译链接全部固件，并检查 Flash 双槽 map 地址。
- `tests/firmware_host/test_core.c` 当前只参加交叉编译，不在 Windows 主机上执行。本文不把交叉编译说成 MCU 硬件行为测试。
- UART 电平、真实中断时序、电机/编码器方向、实际堵转阈值、灰度有效电平、Flash 断电回退和赛道表现只能通过实物确认。本次按用户要求豁免，完整保留在 `docs/hardware-test-checklist.md`，不阻塞软件完成。
- 协议中的 `EVENT` 类型是向后兼容的扩展保留项，不是首版必需命令，也不代表存在未完成的首版命令。
- 电池 ADC、电流检测、MPU6050 闭环和直角专用状态机按批准方案属于后加项，不计入首版软件完成条件。

## 结论规则

只有在当前提交重新运行 `tools/verify.ps1` 成功、`git diff --check` 成功、未发现未完成占位词遗留并确认 GitHub `origin/main` 与本地提交一致后，才把本次软件交付记为完成。实物验收状态保持未执行，不做虚假通过声明。
