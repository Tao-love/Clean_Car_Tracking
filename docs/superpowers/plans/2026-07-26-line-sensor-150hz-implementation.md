# 150 Hz 灰度与控制周期改造 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将灰度、循线与速度控制从 100 Hz 升至 150 Hz，保持物理车速与剩余安全时间，并删除通信超时停车。

**Architecture:** TIMG0 是唯一控制时基，`empty.syscfg` 的周期改为 6.667 ms。每周期速度量按 2/3 换算，时间型阈值按 3/2 换算；通信超时的枚举、安全状态、API、协议调用和测试一并删除。

**Tech Stack:** MSPM0G3507、TI SysConfig、TI DriverLib、CCS/ticlang、PowerShell。

## Global Constraints

- 仅修改 `empty.syscfg`、`src/`、`inc/` 和 `tests/`；绝不手改 `syscfg_gen/` 或 `Debug/`。
- TIMG0 仍使用 LFCLK；PWM 上限仍为 300。
- 保留丢线、堵转、参数错误和控制超期保护；不再有任何通信心跳停车。
- 用户自行烧录，先悬空测试。

---

### Task 1: 时基与安全时间

**Files:**
- Modify: `empty.syscfg:174-179`, `inc/trial_manager.h:3-6,22`, `inc/safety_guard.h:3-20`, `src/main.c:3-9,44-47`, `src/trial_manager.c:1-2`, `tests/verify_trial_duration.ps1:1-15`
- Create: `tests/verify_control_rate.ps1`

**Interfaces:** 每个 TIMG0 ZERO 事件是一个 6.667 ms 控制 tick；试验为 2250 tick，丢线和堵转阈值为 23 和 45 tick。

- [ ] **Step 1: Write a failing rate contract**

Create `tests/verify_control_rate.ps1`:

```powershell
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$cfg = Get-Content -LiteralPath (Join-Path $root 'empty.syscfg') -Raw
$safety = Get-Content -LiteralPath (Join-Path $root 'inc\safety_guard.h') -Raw
if ($cfg -notmatch 'TIMER_CONTROL\.timerPeriod\s*=\s*"6\.667 ms";') { throw '控制周期必须为 6.667 ms。' }
if ($safety -notmatch '#define\s+SAFETY_LINE_LOST_TICKS\s+\(23U\)') { throw '丢线保护必须保持约 150 ms。' }
if ($safety -notmatch '#define\s+SAFETY_STALL_TICKS\s+\(45U\)') { throw '堵转保护必须保持约 300 ms。' }
Write-Host '150 Hz control-rate contract passed.'
```

- [ ] **Step 2: Verify the new test fails**

Run `powershell -ExecutionPolicy Bypass -File .\tests\verify_control_rate.ps1`.

Expected: failure because the timer remains 10 ms.

- [ ] **Step 3: Implement and validate time conversion**

Set:

```js
TIMER_CONTROL.timerPeriod = "6.667 ms";
```

```c
#define TRIAL_DURATION_TICKS (2250U)
#define SAFETY_LINE_LOST_TICKS (23U)
#define SAFETY_STALL_TICKS     (45U)
```

Update control-path comments to 6.667 ms/150 Hz and have `verify_trial_duration.ps1` require `2250U`.

Run `powershell -ExecutionPolicy Bypass -File .\tests\verify_control_rate.ps1; powershell -ExecutionPolicy Bypass -File .\tests\verify_trial_duration.ps1`.

Expected: both print `passed`.

- [ ] **Step 4: Commit**

```powershell
git add empty.syscfg inc/trial_manager.h inc/safety_guard.h src/main.c src/trial_manager.c tests/verify_control_rate.ps1 tests/verify_trial_duration.ps1
git commit -m "feat: run line control at 150 Hz"
```

### Task 2: 速度、PD 与遥测换算

**Files:**
- Modify: `src/manual_tuning.c:2-26`, `src/app_protocol.c:146-154`, `inc/autotune_types.h:3-7`, `inc/gray_sensor.h:3-6`, `inc/line_control.h:3-6`, `inc/speed_pi.h:3-6`, `inc/encoder.h:3-6`, `inc/trial_stats.h:3-6`, `tests/verify_manual_tuning.ps1:33-34`

**Interfaces:** `LineControl_Step` 与 `SpeedPI_Step` 接收每 6.667 ms 的速度，输出约等于旧固件的物理直线速度与修正强度。

- [ ] **Step 1: Make the tuning contract fail for the target values**

In `tests/verify_manual_tuning.ps1`, require this source fragment:

```powershell
Require-Match $TuningSource '12L \* Q16_ONE, 0, 42L \* Q16_ONE,' '左速度参数必须匹配 150 Hz 换算。'
Require-Match $TuningSource '12L \* Q16_ONE, 0, \(69L \* Q16_ONE / 2\),' '右速度参数必须匹配 150 Hz 换算。'
Require-Match $TuningSource '\(Q16_ONE / 84\), \(Q16_ONE / 448\),' '循线 PD 必须匹配 150 Hz 换算。'
Require-Match $TuningSource '\(5L \* Q16_ONE / 8\), 10000,' 'D 滤波时间常数必须保持。'
Require-Match $TuningSource '27, 67, 67,' '目标速度必须换算为每 6.667 ms 计数。'
Require-Match $TuningSource '300, 2333,' 'D 差分限幅必须匹配 150 Hz。'
Require-Match $TuningSource '10U, 5U' '控制超期限值必须保持近似时间。'
```

- [ ] **Step 2: Verify it fails, then replace the parameter initializer**

Run `powershell -ExecutionPolicy Bypass -File .\tests\verify_manual_tuning.ps1`; it must fail before source edits.

Replace `gManualParams` numeric fields with:

```c
12L * Q16_ONE, 0, 42L * Q16_ONE,
12L * Q16_ONE, 0, (69L * Q16_ONE / 2),
(Q16_ONE / 84), (Q16_ONE / 448),
(5L * Q16_ONE / 8), 10000,
27, 67, 67,
300, 2333,
80, 1,
10U, 5U
```

Replace `uint32_t interval = 100U / gTelemetryHz;` with `uint32_t interval = 150U / gTelemetryHz;`, and update affected comments to “每 6.667 ms 编码器计数”.

- [ ] **Step 3: Verify and commit**

Run `powershell -ExecutionPolicy Bypass -File .\tests\verify_manual_tuning.ps1`.

Expected: `Manual tuning source contract passed.`

```powershell
git add src/manual_tuning.c src/app_protocol.c inc/autotune_types.h inc/gray_sensor.h inc/line_control.h inc/speed_pi.h inc/encoder.h inc/trial_stats.h tests/verify_manual_tuning.ps1
git commit -m "feat: scale line tuning for 150 Hz"
```

### Task 3: 完全移除通信超时

**Files:**
- Modify: `inc/autotune_types.h:29-50`, `inc/safety_guard.h:18-46`, `src/safety_guard.c:16-60,135-150`, `inc/trial_manager.h:56-58`, `src/trial_manager.c:140-157`, `src/app_protocol.c:116-119,160-164`, `tests/firmware_host/test_core.c:341-374`, `tests/verify_control_rate.ps1`

**Interfaces:** `SafetyGuard_Evaluate` 变为 `bool SafetyGuard_Evaluate(uint32_t tick, const ControlSample *sample, const ControlParams *params, bool requireLine);`，只处理参数、丢线和左右堵转。

- [ ] **Step 1: Add failing absence assertions**

Append to `verify_control_rate.ps1`:

```powershell
$safetySource = Get-Content -LiteralPath (Join-Path $root 'src\safety_guard.c') -Raw
$protocolSource = Get-Content -LiteralPath (Join-Path $root 'src\app_protocol.c') -Raw
if ($safetySource -match 'COMM_TIMEOUT|lastValidFrameTick|OnValidFrame|requireCommHeartbeat') { throw '通信超时安全路径必须彻底移除。' }
if ($protocolSource -match 'TrialManager_OnValidFrame') { throw '协议层不得再刷新通信心跳。' }
```

- [ ] **Step 2: Verify failure, then delete the exact feature surface**

Run `powershell -ExecutionPolicy Bypass -File .\tests\verify_control_rate.ps1`; it must fail before deletion.

Delete `SAFETY_COMM_TIMEOUT_TICKS`, `lastValidFrameTick`, `SafetyGuard_OnValidFrame`, `TrialManager_OnValidFrame`, both protocol callers, and the heartbeat `if` branch. Delete `STOP_REASON_COMM_TIMEOUT` and `FAULT_COMM_TIMEOUT`, renumber subsequent enum values. Change every declaration, definition, and caller to the four-argument `SafetyGuard_Evaluate` signature. Delete only the host-test timeout assertions at ticks 39/40 and the fifth-argument bypass test; retain line-loss, stall, and overrun tests.

- [ ] **Step 3: Verify and commit**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\verify_control_rate.ps1
powershell -ExecutionPolicy Bypass -File .\tests\verify_manual_tuning.ps1
powershell -ExecutionPolicy Bypass -File .\tests\verify_trial_duration.ps1
powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1
```

Expected: every command exits 0 and source has no removed-heartbeat identifiers.

```powershell
git add inc/autotune_types.h inc/safety_guard.h src/safety_guard.c inc/trial_manager.h src/trial_manager.c src/app_protocol.c tests/firmware_host/test_core.c tests/verify_control_rate.ps1
git commit -m "feat: remove communication timeout safety stop"
```

### Task 4: Regenerate, build, and hardware handoff

**Files:**
- Regenerate only: `syscfg_gen/`
- Build only: `Debug/`

- [ ] **Step 1: Regenerate SysConfig**

```powershell
& 'C:\Ti\sysconfig_1.26.2\sysconfig_cli.bat' --script 'E:\TI_work\TI_Project\Clean_Car\empty.syscfg' -o 'syscfg_gen' --product 'C:\ti\ti\mspm0_sdk_2_11_00_07\.metadata\product.json' --compiler ticlang
```

Expected: exit 0; do not hand-edit the generated result.

- [ ] **Step 2: Build and run final checks**

```powershell
gmake -C .\Debug clean all
powershell -ExecutionPolicy Bypass -File .\tests\verify_control_rate.ps1
powershell -ExecutionPolicy Bypass -File .\tests\verify_manual_tuning.ps1
powershell -ExecutionPolicy Bypass -File .\tests\verify_trial_duration.ps1
powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1
git diff --check
```

Expected: build/checks exit 0 and `git diff --check` prints nothing.

- [ ] **Step 3: Commit and hand off**

```powershell
git add empty.syscfg inc src tests docs/superpowers/plans/2026-07-26-line-sensor-150hz-implementation.md
git commit -m "test: verify 150 Hz line control migration"
```

User manually flashes the intended output, tests wheels suspended first, then tests on line. Expected visible result: fresh correction every 6.7 ms rather than 10 ms with close-to-previous straight speed. On reversal, stall, or line departure, disconnect motor power and report the last visible behavior.
