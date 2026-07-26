# 蓝牙主机层与文档清理 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 删除 Python 蓝牙自动调参层及其文档入口，保留 MSPM0 控制固件、UART1 硬件配置和所有 SysConfig 生成文件不变。

**Architecture:** 本计划移除电脑端 `autotune` 软件、配置与测试，并删除或改写 JDY-31 和蓝牙协议的项目文档。固件的 `app_protocol`、`protocol`、`uart_transport` 和 UART1 暂不删除，后续 MCU 蓝牙控制层任务再把 UART1 改为 USB-TTL 回显。

**Tech Stack:** Git、PowerShell、Markdown、MSPM0 CCS 工程元数据。

## Global Constraints

- 不修改 `syscfg_gen/`、`Debug/`、`.build/` 或其他 SysConfig 生成文件。
- 不编译、链接、运行单元测试、验证脚本或烧录；只执行用户指定的静态 `rg` 残留检查。
- 不删除 `tests/firmware_host/test_core.c`，它不是 Python 自动调参测试。
- 不修改用户 Obsidian Vault 中的外部蓝牙方案文档。
- 本轮 UART1 固件仍是二进制协议传输；README 不得宣称已实现文本回显。

---

### Task 1: 删除 PC 自动调参层

**Files:**

- Delete: `python/autotune/__init__.py`, `python/autotune/__main__.py`, `python/autotune/cli.py`, `python/autotune/config.py`, `python/autotune/flash_record.py`, `python/autotune/link.py`, `python/autotune/logging.py`, `python/autotune/messages.py`, `python/autotune/models.py`, `python/autotune/optimizer.py`, `python/autotune/protocol.py`, `python/autotune/scoring.py`, `python/autotune/session.py`, `python/autotune/stages.py`, `python/autotune/trial.py`, `python/autotune/vofa.py`.
- Delete: `python/config.example.json`, `python/requirements.txt`, `python/tests/test_flash_record.py`, `python/tests/test_link.py`, `python/tests/test_models.py`, `python/tests/test_optimizer.py`, `python/tests/test_protocol.py`, `python/tests/test_scoring.py`, `python/tests/test_session.py`, `python/tests/test_trial.py`, `python/tests/test_vofa.py`.
- Modify: `tools/verify.ps1:10-17,25-30,43-48`.

**Interfaces:**

- Consumes: 当前仅被 PC 自动调参 CLI 使用的 `pyserial`、`numpy`、`autotune` 包和 Python 测试。
- Produces: 仓库不再包含打开蓝牙 COM、下发命令、自动搜索、日志、VOFA+ 转发或 Python Flash 提交入口。

- [ ] **Step 1: 删除已跟踪的 Python 自动调参文件**

用 `apply_patch` 删除上列 Python 文件；不删除未跟踪的 `__pycache__` 文件，也不执行 Python 命令。

- [ ] **Step 2: 移除验证脚本中的 Python 专属步骤**

从 `tools/verify.ps1` 删除 `$PythonExe` 参数、必需文件列表中的 `$PythonExe`，以及 `PYTHONPATH`、`unittest discover` 和 `compileall python\\autotune` 四行。脚本头部职责改为只描述 SysConfig、固件编译/链接和 Flash 地址检查，其余编译行为不改。

- [ ] **Step 3: 记录影响**

报告：`python -m autotune`、COM 自动调参、评分、CSV/事件日志、VOFA+ 转发和 `commit` Flash 命令已删除；MCU 端协议仍暂时存在但无仓库内 PC 客户端。

- [ ] **Step 4: Commit**

执行 `git add -u python tools/verify.ps1`，然后提交信息 `remove: delete Bluetooth autotune host`。

### Task 2: 清理蓝牙协议与验收文档

**Files:**

- Delete: `docs/protocol.md`, `docs/hardware-test-checklist.md`, `docs/deviations.md`, `docs/software-completion-audit.md`, `docs/specs/1-3.md`, `docs/specs/1-3-summary.md`, `docs/superpowers/plans/2026-07-20-1-3-implementation.md`, `docs/superpowers/specs/2026-07-24-bluetooth-removal-design.md`.
- Modify: `README.md`, `empty.syscfg:184`, `docs/superpowers/specs/2026-07-24-manual-tuning-design.md:30-42`, `docs/superpowers/specs/2026-07-21-usb-uart-bsl-design.md:17-35`, `docs/superpowers/plans/2026-07-21-usb-uart-bsl-implementation.md:4-16,191-226`.

**Interfaces:**

- Consumes: 已删除的主机端命令、JDY-31 接线和二进制协议均不再是支持功能。
- Produces: README 只保留手动 KEY1、安全限制、当前源码参数位置、USB-TTL 物理接线和 CH340 UART BSL 下载说明。

- [ ] **Step 1: 删除只服务蓝牙自动调参的文档**

用 `apply_patch` 删除上列项目内文档。保留手动调参设计；其中“蓝牙”改成“历史远程参数/历史 Flash 参数”。

- [ ] **Step 2: 重写 README 为手动调参入口**

保留工程说明、KEY1 手动循线、安全限制、当前 `src/control_params.c` 参数位置、`USART1` USB-TTL 接线、CH340 UART BSL 下载和目录说明。删除 JDY-31、Python、`autotune`、VOFA+、协议试验、蓝牙验收和失效链接；明确 UART1 仍是二进制协议，文本回显和集中手动参数表属于后续 MCU 任务。

- [ ] **Step 3: 清理保留文档和 SysConfig 源注释**

把 `empty.syscfg` 的 JDY-31 注释改为“UART1 使用 9600 8N1；RX 中断只搬运字节，不在 ISR 解析上层数据”，不得改动 UART 实例、引脚、波特率或中断字段。USB BSL 文档中的“蓝牙 COM/JDY-31”改成“非 CH340 的其他串口”，不改 BSL 配置。

- [ ] **Step 4: 记录影响并提交**

报告：仓库不再提供 JDY-31 接线、二进制协议、蓝牙验收或自动调参教程；USB-TTL 物理接线与 CH340 下载边界保留。执行 `git add -u docs README.md empty.syscfg`，提交信息 `docs: remove Bluetooth operation guides`。

### Task 3: 静态残留检查

**Files:**

- Inspect: 全部已跟踪工程文件，排除 `Debug/`、`.build/`、`syscfg_gen/` 和 `work_filed/`。

**Interfaces:**

- Consumes: 前两任务后的 Git 工作树。
- Produces: 残留蓝牙名称清单；MCU 固件中待后续删除的协议符号单独报告。

- [ ] **Step 1: 检查已删除主机端路径**

运行 `git ls-files python`，预期无输出。

- [ ] **Step 2: 检查用户入口文档残留**

运行 `rg -n -i -S 'JDY-31|蓝牙自动调参|python -m autotune|VOFA\\+' README.md docs tools --glob '!docs/superpowers/plans/2026-07-24-remove-bluetooth-host-and-docs.md'`，预期无输出；当前执行计划是过程记录，排除在用户入口文档检查外。

- [ ] **Step 3: 单列固件待处理项**

运行 `rg -l -i -S 'bluetooth|JDY|protocol|AppProtocol|TRIAL_START_SOURCE_BLUETOOTH' src inc`。预期可能列出 `src/app_protocol.c`、`src/protocol.c`、`src/trial_manager.c` 和对应头文件；它们属于尚未开始的 MCU 蓝牙控制层，不作为本轮失败。

- [ ] **Step 4: 结束**

静态检查不改文件，不创建额外提交。
