# Manual Source Tuning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the USB_nobluetooth firmware run repeatable, KEY1-initiated five-second line-following trials whose initial parameters come only from a human-editable source table.

**Architecture:** Add a small `manual_tuning` module that owns the editable `ControlParams` baseline. `ControlParams_Init` always copies that baseline and ignores historical Flash input. The application starts `TRIAL_MODE_LINE_FOLLOW` from KEY1; normal completion returns to `IDLE`, while safety faults remain latched until reset.

**Tech Stack:** C17-compatible TI Arm Clang, MSPM0 DriverLib/SysConfig, existing firmware contract tests, PowerShell source-contract test.

## Global Constraints

- Do not modify `empty.syscfg` or generated `syscfg_gen`/`Debug` files.
- Keep the 100 Hz loop, 500-tick limit, line-loss, stall, and control-overrun protections.
- `maxPwm` remains no greater than 400; the manual baseline remains 200.
- A safety fault must not be cleared by KEY1; reset/power-cycle is required before another test.
- UART2 stays in raw-echo mode and is not part of the tuning workflow.

---

### Task 1: Define and verify the manual parameter source

**Files:**
- Create: `inc/manual_tuning.h`
- Create: `src/manual_tuning.c`
- Modify: `src/control_params.c`
- Modify: `tests/firmware_host/test_core.c`
- Modify: `tools/verify.ps1`

**Interfaces:**
- Produces: `const ControlParams *ManualTuning_GetParams(void)`.
- Consumes: `ControlParams_Validate(const ControlParams *)` and `Q16_ONE`.

- [ ] **Step 1: Write a failing contract test**

Add `manual_tuning.h` to `tests/firmware_host/test_core.c`. Add `Test_ManualTuning` which obtains `ManualTuning_GetParams()`, rejects a null pointer, and requires `ControlParams_Validate(params) == PARAMS_VALID`, `params->baseSpeed == 5`, and `params->maxPwm == 200`.

- [ ] **Step 2: Run the validation and observe failure**

Run: `powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1`

Expected: compilation fails because `manual_tuning.h` and `ManualTuning_GetParams` do not exist.

- [ ] **Step 3: Add the minimal manual source module**

Create the header declaration and a single `static const ControlParams` in `src/manual_tuning.c`, with the existing conservative values: speed Kp `8/8`, Ki `0/0`, feedforward `28/24`, line Kp/Kd `0/0`, base speed `5`, max target/delta `100/100`, max PWM `200`, derivative alpha `0.5`, integral limit `10000`, derivative limit `3500`, stall `80/1`, telemetry `10`, and overrun limit `3`.

Make `ControlParams_Init` copy this value even when a valid Flash record is supplied; retain the existing validation fallback.

- [ ] **Step 4: Run the validation and observe success**

Run: `powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1`

Expected: SysConfig and all sources compile and link; the output ends with `验证通过:`.

### Task 2: Start a line-following manual trial and permit only normal retries

**Files:**
- Modify: `src/main.c`
- Modify: `src/trial_manager.c`
- Create: `tests/verify_manual_tuning.ps1`

**Interfaces:**
- Consumes: `TrialManager_Start(tick, TRIAL_MODE_LINE_FOLLOW, 0, 0, TRIAL_START_SOURCE_KEY1)`.
- Produces: source-level contract that proves Flash loading is absent from the application start path and normal completion returns to `IDLE`.

- [ ] **Step 1: Write a failing source-contract test**

Create `tests/verify_manual_tuning.ps1` that requires `src/main.c` to call `TrialManager_Init(0U, 0, 0U)`, to start `TRIAL_MODE_LINE_FOLLOW` with `0, 0`, and not to contain `FlashParams_LoadLatest`; require `src/trial_manager.c` to assign `SYSTEM_STATE_IDLE` only for `STOP_REASON_TRIAL_COMPLETE` and preserve the requested state for faults.

- [ ] **Step 2: Run the source-contract test and observe failure**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\verify_manual_tuning.ps1`

Expected: failure reporting the existing Flash load and `TRIAL_MODE_WHEEL_SPEED` startup.

- [ ] **Step 3: Apply the minimal control-flow changes**

Remove Flash loading from `main.c`, initialize `TrialManager` with null parameters, and start line-follow mode from KEY1. In `TrialManager_Finish`, replace `FINISHED` with `IDLE` only when the stop reason is `STOP_REASON_TRIAL_COMPLETE`; leave any fault state unchanged.

- [ ] **Step 4: Run both validation layers**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\verify_manual_tuning.ps1; powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1`

Expected: the source contract prints `Manual tuning source contract passed.` and the firmware build prints `验证通过:`.

### Task 3: Replace the Bluetooth workflow with the manual burn-and-observe workflow

**Files:**
- Modify: `README.md`
- Modify: `C:\Users\ZhuanZ（无密码）\Documents\Obsidian Vault\pid调参AI\运行流程细化\运行流程.md`

**Interfaces:**
- Consumes: the manual table in `src/manual_tuning.c`, KEY1 line-follow behavior, and safety timings.
- Produces: a documented trial-report template and parameter-change order.

- [ ] **Step 1: Document the manual sequence**

Document the exact sequence: edit one justified parameter group in `src/manual_tuning.c`, build and burn, power-cycle after any fault, verify wheel direction suspended, then run one low-speed on-track five-second KEY1 trial. Require reporting the actual burned parameters, setup, duration, and stop phenomenon.

- [ ] **Step 2: Document safe tuning order and fault handling**

Specify speed-loop/feedforward verification before line PD, line Kp then Kd, then `baseSpeed` increases. Explain `baseSpeed` units, PWM limits, and the 0.15 s/0.3 s/5 s diagnostic timing. State that faults are hardware/sensor checks first, not a cue to increase gains.

- [ ] **Step 3: Re-run the source contract**

Run: `powershell -ExecutionPolicy Bypass -File .\tests\verify_manual_tuning.ps1`

Expected: `Manual tuning source contract passed.`
