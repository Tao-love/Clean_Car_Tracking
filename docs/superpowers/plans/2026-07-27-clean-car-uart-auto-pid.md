# Clean_Car UART Auto PID Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a 115200 UART2 Clean_Car adapter that implements the committed Clean_Car host profile without changing KEY1-only motor control.

**Architecture:** `UART_Auto_PID.c` owns nonblocking line RX, command validation, XOR framing, TX FIFO, and telemetry formatting. `main.c` only services the module; `line_run.c` only supplies a control snapshot and resets state after an accepted gain update. The implementation deliberately follows `firmware.cpp`'s line-buffer, candidate-then-commit, reset-history, and CSV-report structure, but maps it to the real two speed PI loops and line PD.

**Tech Stack:** C11, TI MSPM0 DriverLib UART2, existing CCS/SysConfig project, clang host tests.

## Global Constraints

- UART is UART2, PB15 TX, PB16 RX, 115200 8N1; SysConfig is the source of truth.
- Do not edit generated SysConfig sources by hand.
- Do not format, poll, or transmit UART data in `LineRun_ControlTick()`.
- RX accepts only `STATUS` and a checksummed complete six-gain `SETALL`; it never starts or stops the motor.
- A valid write resets both speed PI integrators and line-control history.
- Telemetry is a checksummed 21-column CSV frame, at most 128 ASCII bytes, generated once every three control ticks.

### Task 1: Testable protocol core

**Files:** Create `inc/UART_Auto_PID.h`, `src/UART_Auto_PID.c`, `tests/firmware_host/test_uart_auto_pid.c`; modify `tools/verify.ps1`.

**Interfaces:** `UART_Auto_PID_Init`, `UART_Auto_PID_ProcessLineForTest`, and `UART_Auto_PID_OnControlSample` expose the parser and telemetry scheduler without requiring physical UART in host tests.

- [ ] Write failing host tests for: valid checksummed `SETALL` changes all six Q16 gains; bad checksum, duplicate field, missing field, range failure, and long line leave all gains unchanged; accepted parameters request a control-state reset; telemetry is ready only on every third snapshot.
- [ ] Run the new host test and confirm that it fails because the module does not exist.
- [ ] Implement the smallest parser, XOR helper, fixed-point decimal conversion, atomic candidate commit, and telemetry-ready counter to pass it.
- [ ] Run the host test and confirm it passes.

### Task 2: Clean_Car control integration

**Files:** Modify `src/control_params.c`, `inc/control_params.h`, `src/line_run.c`, `inc/line_run.h`, `src/main.c`; modify `tests/firmware_host/test_line_run.c`.

**Interfaces:** `ControlParams_GetMutableForUart()` returns the active parameter table only to the UART module. `LineRun_ResetForTuningUpdate()` resets existing internal PI/line state. `UART_Auto_PID_OnControlSample()` receives already-computed control values after `Motor_SetSpeed`.

- [ ] Write failing tests that prove an accepted update resets both PI states and line state, while KEY1 remains the only call path to `LineRun_Start`/`LineRun_Stop`.
- [ ] Run the test and confirm the missing integration fails.
- [ ] Implement minimal non-serial glue: make the active parameter table mutable, provide the reset entry point, copy the control snapshot after the normal control calculation, and call UART init/service from the main loop.
- [ ] Run host tests and confirm they pass.

### Task 3: DriverLib service, SysConfig contract, and verification

**Files:** Modify `src/UART_Auto_PID.c`, `tests/verify_uart2_syscfg.ps1`, `tools/verify.ps1`; regenerate generated outputs from existing `empty.syscfg` through the existing verifier.

**Interfaces:** `UART_Auto_PID_Service()` drains RX FIFO into the line parser and drains the software TX FIFO into UART2 only while hardware FIFO space exists.

- [ ] Add a failing source-contract test requiring 115200, runtime UART module use, no UART calls in `LineRun_ControlTick`, `STATUS`, checksummed `SETALL`, `ACK`, `ERR`, and a 21-column telemetry formatter.
- [ ] Run it and confirm failure against the old 9600/no-runtime-UART contract.
- [ ] Implement nonblocking DriverLib RX/TX service; `STATUS` queues one snapshot only and no command changes running state.
- [ ] Regenerate SysConfig outputs from the user's existing 115200 `empty.syscfg`, run the complete verifier and the CCS build, then inspect the diff without overwriting unrelated user changes.

### Verification and handoff

- [ ] In `E:\电赛\pid调参\llm-pid-tuner-2.4.7\config.json`, switch `HARDWARE_PROFILE` to `generic_serial_csv_clean_car` only after firmware build passes.
- [ ] With wheels suspended, verify one `STATUS` reply, rejected corrupt `SETALL`, accepted valid `SETALL` plus matching `ACK`, then 50 Hz protected telemetry.
- [ ] Commit source, headers, tests, regenerated SysConfig outputs, and documentation only after `tools/verify.ps1` and the build pass. Do not commit unrelated `empty.syscfg` changes without user authorization.
