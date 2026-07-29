# Copy Hand-Tracking Stable Parameters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Legacy/Clean_Car start with the verified runtime parameter values from Clear_Car_Hand_Tracking while keeping Legacy's UART parameter-write capability.

**Architecture:** Replace only the initial `gControlParams` values in `src/control_params.c`. Preserve the target project's mutable parameter accessor and validation ceilings, so the manual serial host can still write its six gains after startup. Add one source-contract script that checks the new defaults and absence of obsolete exported-gain macros.

**Tech Stack:** C, PowerShell static source contract, existing CCS Debug build.

## Global Constraints

- Do not modify UART protocol, SysConfig, motor, encoder, control-loop timing, or Flash behavior.
- Preserve `ControlParams_GetMutableForUart()` and target validation ceilings.
- Copy the source runtime defaults exactly: left `12/0/42`, right `12/0/34.5`, line `1/70` and `1/320`, alpha `5/8`, integral limit `10000`, `baseSpeed=27`, targets `67/67`, `maxPwm=300`, derivative limit `2333`.
- Do not flash, open a serial port, or run the motors.

---

### Task 1: Replace Legacy startup defaults with the stable parameter set

**Files:**
- Create: `tests/verify_hand_tracking_stable_params.ps1`
- Modify: `src/control_params.c`

**Interfaces:**
- Consumes: `ControlParams` field order defined in `inc/control_types.h` and the source project's verified initializer.
- Produces: a mutable Legacy `gControlParams` whose startup values match the stable source set and whose six old `CLEAN_CAR_EXPORTED_*` macros no longer exist.

- [ ] **Step 1: Write the failing stable-default contract**

Create `tests/verify_hand_tracking_stable_params.ps1` with:

```powershell
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\control_params.c')

foreach ($pattern in @(
    '12L\s*\*\s*Q16_ONE,\s*0,\s*42L\s*\*\s*Q16_ONE,',
    '12L\s*\*\s*Q16_ONE,\s*0,\s*\(69L\s*\*\s*Q16_ONE\s*/\s*2\),',
    '\(Q16_ONE\s*/\s*70\),\s*\(Q16_ONE\s*/\s*320\),',
    '\(5L\s*\*\s*Q16_ONE\s*/\s*8\),\s*10000,',
    '27,\s*67,\s*67,',
    '300,\s*2333'
)) {
    if ($Source -notmatch $pattern) { throw "稳定默认参数不匹配: $pattern" }
}
if ($Source -match 'CLEAN_CAR_EXPORTED_') {
    throw '旧自动整定参数宏仍存在。'
}
if ($Source -notmatch 'ControlParams\s*\*ControlParams_GetMutableForUart\(void\)') {
    throw 'UART 可写参数访问接口缺失。'
}
Write-Host 'Hand-tracking stable parameter contract passed.'
```

- [ ] **Step 2: Run the contract to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File tests\verify_hand_tracking_stable_params.ps1`

Expected: failure because the current target initializer still uses `CLEAN_CAR_EXPORTED_*` and `baseSpeed=5`.

- [ ] **Step 3: Replace only the obsolete defaults**

In `src/control_params.c`, delete the six `CLEAN_CAR_EXPORTED_*` definitions and replace the initializer with:

```c
static ControlParams gControlParams = {
    12L * Q16_ONE, 0, 42L * Q16_ONE,
    12L * Q16_ONE, 0, (69L * Q16_ONE / 2),
    (Q16_ONE / 70), (Q16_ONE / 320),
    (5L * Q16_ONE / 8), 10000,
    27, 67, 67,
    300, 2333
};
```

Do not change `PARAM_GAIN_MAX_Q16`, `PARAM_PWM_MAX`, `ControlParams_IsValid`, or `ControlParams_GetMutableForUart()`.

- [ ] **Step 4: Verify the contract and compile**

Run: `powershell -ExecutionPolicy Bypass -File tests\verify_hand_tracking_stable_params.ps1`

Expected: `Hand-tracking stable parameter contract passed.`

Run: `& 'C:\ti\ccs\utils\bin\gmake.exe' -C Debug -B all`

Expected: exit code 0 and `Debug\Clean_Car.out` produced. This is source/build verification only.

- [ ] **Step 5: Commit only task files**

```powershell
git add -- src/control_params.c tests/verify_hand_tracking_stable_params.ps1
git commit -m "feat: use hand-tracking stable defaults"
```

## Self-review

- Spec coverage: the task updates all 15 startup values, removes the obsolete exported macros, preserves UART mutability and leaves all excluded control modules untouched.
- Placeholder scan: no deferred implementation items remain.
- Type consistency: the plan only uses the existing `ControlParams` initializer and existing mutable accessor.
