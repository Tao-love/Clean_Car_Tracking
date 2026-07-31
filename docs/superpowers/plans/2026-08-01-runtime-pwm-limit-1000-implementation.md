# Runtime PWM Ceiling 1000 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow a complete wireless `SETALL` update to set `PWM` from 1 through 1000 in runtime RAM while retaining the compiled default of 600.

**Architecture:** The firmware has two independent validation boundaries: `ControlParams_IsValid()` protects the parameter store and `UART_Auto_PID_GainsAreValid()` protects wireless updates. Both must use the same maximum value. The existing UART firmware-host contract test will submit an allowed `PWM:1000` snapshot and reject an otherwise-valid `PWM:1001` snapshot atomically.

**Tech Stack:** C11-compatible TI Arm Clang firmware, CCS/SysConfig project, PowerShell verification script.

## Global Constraints

- Change only the runtime PWM validation ceiling from 600 to 1000.
- Keep the compiled default `gControlParams.maxPwm` at 600.
- Keep motor hardware limiting at `MOTOR_PWM_PERIOD=1600` unchanged.
- Do not edit generated `syscfg_gen/`, `Debug/`, or build output files.
- Do not flash, open a serial port, or run the motors.
- Preserve all pre-existing uncommitted edits outside the explicitly listed lines.

---

### Task 1: Add wireless PWM boundary coverage

**Files:**

- Modify: `E:/TI_work/TI_Project/Legacy/Clean_Car/tests/firmware_host/test_uart_auto_pid.c:109-148`

**Interfaces:**

- Consumes: `UART_Auto_PID_ProcessLineForTest(const char *line)`, which returns `true` only for an accepted protocol frame.
- Produces: Test coverage that requires `PWM:1000` to replace `params.maxPwm` and requires `PWM:1001` to leave the complete `ControlParams` snapshot unchanged.

- [ ] **Step 1: Update the success-case frame to request PWM 1000**

In `Test_AcceptsCompleteSetAll`, replace the frame tail and expected value with:

```c
"TARGET:67 DELTA:35 PWM:1000 DLIMIT:2000");
...
(params.maxPwm != 1000) ||
```

- [ ] **Step 2: Add the rejected-1001 atomicity case**

At the end of `Test_InvalidFramesAreAtomic`, before `return 0;`, add this complete frame and assertions:

```c
Test_Frame(line, sizeof(line),
    "SETALL SEQ:2 LKP:1 LKI:0 LFF:1 RKP:1 RKI:0 RFF:1 "
    "LINEP:0 LINED:0 ALPHA:0.625 ILIMIT:10000 BASE:15 "
    "TARGET:67 DELTA:35 PWM:1001 DLIMIT:2333");
if (UART_Auto_PID_ProcessLineForTest(line) ||
    (memcmp(&params, &unchanged, sizeof(params)) != 0) ||
    (gResetRequests != 0U)) {
    return 15;
}
```

- [ ] **Step 3: Compile the changed contract test against the current ceiling**

Run:

```powershell
Set-Location -LiteralPath 'E:\TI_work\TI_Project\Legacy\Clean_Car'
powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1
```

Expected before Task 2: the ARM contract test compiles and links, but the source still semantically rejects `PWM:1000`; record this limitation because these ARM ELFs are not executed on the PC.

### Task 2: Raise both firmware validation ceilings

**Files:**

- Modify: `E:/TI_work/TI_Project/Legacy/Clean_Car/src/control_params.c:13`
- Modify: `E:/TI_work/TI_Project/Legacy/Clean_Car/src/UART_Auto_PID.c:14`

**Interfaces:**

- Consumes: `ControlParams.maxPwm` as a signed 16-bit runtime PWM limit.
- Produces: `ControlParams_IsValid()` and `UART_Auto_PID_GainsAreValid()` both accept `maxPwm <= 1000` and reject values above 1000.

- [ ] **Step 1: Change the parameter-store maximum**

Replace the definition in `src/control_params.c` with:

```c
#define PARAM_PWM_MAX               (1000)
```

Do not change the default initializer line:

```c
600, 2333
```

- [ ] **Step 2: Change the wireless SETALL maximum**

Replace the definition in `src/UART_Auto_PID.c` with:

```c
#define UART_AUTO_PID_PWM_MAX            (1000)
```

- [ ] **Step 3: Verify source, protocol contract, SysConfig generation, and full ARM build**

Run:

```powershell
Set-Location -LiteralPath 'E:\TI_work\TI_Project\Legacy\Clean_Car'
powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1
& 'C:\ti\ccs\utils\bin\gmake.exe' -C Debug -B all
```

Expected: `tools\verify.ps1` reports `验证通过:` and the CCS build produces `Debug\Clean_Car.out` without modifying the requested motor hardware limit.

- [ ] **Step 4: Inspect the exact final values**

Run:

```powershell
rg -n 'PARAM_PWM_MAX|UART_AUTO_PID_PWM_MAX|600, 2333|MOTOR_PWM_PERIOD' src\control_params.c src\UART_Auto_PID.c inc\motor.h
```

Expected: both validation constants are 1000, the default is still `600, 2333`, and the hardware period is still 1600.

- [ ] **Step 5: Commit only this feature's source and contract-test files**

Run:

```powershell
git add -- src\control_params.c src\UART_Auto_PID.c tests\firmware_host\test_uart_auto_pid.c
git commit -m "feat: allow runtime PWM up to 1000"
```

Do not stage unrelated pre-existing changes.

## Plan Self-Review

- Spec coverage: Task 1 supplies 1000 acceptance and 1001 rejection coverage; Task 2 updates both firmware gates while preserving default 600 and hardware 1600.
- Placeholder scan: no TBD, TODO, or deferred implementation steps.
- Type consistency: the test and both validation gates use the existing signed `int16_t ControlParams.maxPwm` field and existing `UART_Auto_PID_ProcessLineForTest` API.
