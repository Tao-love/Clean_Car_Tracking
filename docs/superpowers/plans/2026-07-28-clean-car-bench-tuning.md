# Clean_Car Bench Tuning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现独立双轮速度台架、固定目标 5 的自动速度调参和手动历史最佳导出退出。

**Architecture:** 固件把 bench 作为 `LineRun` 的并列运行模式并通过 UART 受保护命令启动和更新共同目标。上位机以 Clean_Car speed 专用轮次收集、评分和回退；导出仅由自动三轮收敛或用户确认的历史最佳触发。

**Tech Stack:** MSPM0 C/DriverLib host tests, Python 3, pytest, Textual TUI。

## Global Constraints

- 不改 `empty.syscfg`、`syscfg_gen` 或构建产物。
- 普通 KEY1、RUN、SETALL 和 21 列遥测格式保持兼容。
- 不烧录、不启动电机。
- 速度 D 固定 0；P 为 0--60，I 为 0--0.05，单轮增幅不超过 1.5 倍。

---

### Task 1: 固件台架协议与控制路径

**Files:** `inc/line_run.h`, `src/line_run.c`, `inc/UART_Auto_PID.h`, `src/UART_Auto_PID.c`, `tests/firmware_host/test_line_run.c`, `tests/firmware_host/test_uart_auto_pid.c`。

- [ ] 先为 BENCH、BENCHTARGET、模式隔离和 tick 后同步目标更新写失败的 C 主机测试。
- [ ] 运行既有 firmware_host 构建入口并确认新增断言失败。
- [ ] 以最小接口实现 `LineRun_StartBench`、`LineRun_SetBenchTarget`、模式查询，以及 UART 解析和空闲/运行期校验。
- [ ] 重跑 C 主机测试并确认通过。

### Task 2: 上位机协议、自动轮次与双轮评分

**Files:** `hw/profiles.py`, `hw/serial_bridge.py`, `core/adapters.py`, `core/tuning_engine.py`, `core/tuning_session.py`, 新增或更新 `tests/test_clean_car_wheel_telemetry.py`, `tests/test_tuning_engine.py`, `tests/test_hardware_tui.py`。

- [ ] 先写 BENCH/BENCHTARGET 命令与 ACK、首轮 P=12/I=0、完整遥测、两轮拒绝及三轮门槛的失败 pytest。
- [ ] 运行定位 pytest 并确认它们因缺少功能失败。
- [ ] 增加专用命令、固定目标采样、独立指标与历史最佳回退；排除人工阶跃轮。
- [ ] 重跑定位 pytest 并确认通过。

### Task 3: 手动最佳导出与 TUI 安全退出

**Files:** `core/clean_car_export.py`, `tuner.py`, `sim/tui.py`, `tests/test_clean_car_export.py`, `tests/test_simulator_tui.py`。

- [ ] 先为 `manual_export_best`、不完整快照不导出、确认/取消、运行线程停止和路径提示写失败 pytest。
- [ ] 运行定位 pytest 并确认失败。
- [ ] 实现导出调用与确认对话框，不将报告回写到固件；保留自动收敛唯一的自动导出入口。
- [ ] 重跑定位 pytest 并确认通过。

### Task 4: 完整离线验证

**Files:** 不新增生产文件。

- [ ] 运行上位机 `pytest -q`。
- [ ] 运行 Clean_Car 既有离线验证脚本和 `C:\\ti\\ccs\\utils\\bin\\gmake.exe -C Debug -B all`。
- [ ] 审核差异，确认没有 SysConfig/生成文件改动及每项需求有对应测试。
