# Deferred MPU6050 Recalibration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a future-run MPU6050 recalibration API while disabling all current MPU6050 hardware activity.

**Architecture:** The new public API is a narrow wrapper around the existing initialization and calibration routine. The existing power-on call is removed and no new caller is added, so KEY1, protected `RUN`, and BENCH do not activate MPU6050; its existing disabled fallback returns zero damping.

**Tech Stack:** C11, TI MSPM0 DriverLib project, PowerShell source-contract tests, CCS gmake.

## Global Constraints

- Modify only editable source, header, and test files; never edit `syscfg_gen` or `Debug` output.
- Preserve all existing user changes in the dirty repository.
- Remove the old `MPU6050_InitAndCalibrate()` call from `main.c`; do not call the new API from `main.c`, `line_run.c`, or `UART_Auto_PID.c`.
- Do not flash hardware.

---

### Task 1: Add and verify the deferred public API

**Files:**
- Create: `E:\TI_work\TI_Project\Legacy\Clean_Car\tests\verify_mpu_recalibration.ps1`
- Modify: `E:\TI_work\TI_Project\Legacy\Clean_Car\inc\mpu6050.h`
- Modify: `E:\TI_work\TI_Project\Legacy\Clean_Car\src\mpu6050.c`
- Modify: `E:\TI_work\TI_Project\Legacy\Clean_Car\src\main.c`

**Interfaces:**
- Produces: `void MPU6050_RecalibrateForNextRun(void);`
- Consumes: existing `void MPU6050_InitAndCalibrate(void);`

- [ ] **Step 1: Write the failing test**

Create `tests\verify_mpu_recalibration.ps1` with:

```powershell
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw (Join-Path $root 'inc\mpu6050.h')
$source = Get-Content -Raw (Join-Path $root 'src\mpu6050.c')
if ($header -notmatch 'void\s+MPU6050_RecalibrateForNextRun\(void\);') { throw 'Missing deferred recalibration API declaration.' }
if ($source -notmatch 'void\s+MPU6050_RecalibrateForNextRun\(void\)\s*\{\s*MPU6050_InitAndCalibrate\(\);\s*\}') { throw 'Deferred recalibration must reuse the existing calibration path.' }
foreach ($file in @('src\main.c', 'src\line_run.c', 'src\UART_Auto_PID.c')) {
  if ((Get-Content -Raw (Join-Path $root $file)) -match 'MPU6050_RecalibrateForNextRun\(') { throw "Deferred API must not be called by $file." }
}
$main = Get-Content -Raw (Join-Path $root 'src\main.c')
if ($main -match 'MPU6050_InitAndCalibrate\(\);') { throw 'Power-on MPU calibration must remain disabled.' }
Write-Host 'Deferred MPU recalibration contract passed.'
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\verify_mpu_recalibration.ps1`

Expected: failure reporting the missing declaration.

- [ ] **Step 3: Write minimal implementation**

Add to `inc\mpu6050.h`:

```c
/* Future startup hook: recalibrates only when an explicit caller invokes it. */
void MPU6050_RecalibrateForNextRun(void);
```

Add to `src\mpu6050.c` immediately after `MPU6050_InitAndCalibrate`:

```c
void MPU6050_RecalibrateForNextRun(void)
{
    MPU6050_InitAndCalibrate();
}
```

Remove the `MPU6050_InitAndCalibrate();` statement from `src\main.c`; retain the MPU header include because the future API remains part of the project.

- [ ] **Step 4: Run focused verification**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\verify_mpu_recalibration.ps1`

Expected: `Deferred MPU recalibration contract passed.`

- [ ] **Step 5: Run regression and build verification**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\verify_line_run.ps1`

Expected: existing line-run contract passes.

Run: `C:\ti\ccs\utils\bin\gmake.exe -C Debug -B all`

Expected: exit code 0 and updated `Debug\Clean_Car.out` without flashing.

- [ ] **Step 6: Commit only this task's files**

Run:

```powershell
git add -- 'inc/mpu6050.h' 'src/mpu6050.c' 'tests/verify_mpu_recalibration.ps1' 'docs/superpowers/specs/2026-07-29-deferred-mpu-recalibration-design.md' 'docs/superpowers/plans/2026-07-29-deferred-mpu-recalibration.md'
git commit -m 'feat: add deferred MPU recalibration hook'
```
