# Task 2 report: Clean_Car control integration

## Scope completed

- `src/control_params.c` and `inc/control_params.h`: converted the active
  control table to mutable storage and exposed
  `ControlParams_GetMutableForUart()` for the UART integration only.
- `src/line_run.c` and `inc/line_run.h`: added
  `LineRun_ResetForTuningUpdate()`, which resets the line controller, both
  speed PI states, and held line error without resetting the 10-second run
  timer.  Each running control tick now copies its post-`Motor_SetSpeed`
  values to `UART_Auto_PID_OnControlSample()`.
- `src/main.c`: configures the DriverLib-free UART protocol core after a
  successful `LineRun_Init()`, with a callback that calls the public reset
  entry point.  No UART path calls `LineRun_Start()` or `LineRun_Stop()`;
  KEY1 remains their application call path.
- `tests/firmware_host/test_line_run.c`: adds a composition contract that
  sends a checksum-valid `SETALL` through the actual protocol core and proves
  the accepted update resets one line state and both PI states.

## TDD evidence

### RED

Command (focused strict ARM compile/link):

```powershell
$compiler = 'C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe'
# Compile tests/firmware_host/test_line_run.c, then link it with line_run.o.
```

Result: failed as intended before the integration implementation:

```text
undefined                    first referenced
symbol                       in file
LineRun_ResetForTuningUpdate .build\task2-red\test_line_run.o
error: unresolved symbols remain
```

### GREEN

Command (focused strict ARM compile/link):

```powershell
# Compile src/line_run.c and tests/firmware_host/test_line_run.c with
# -std=c11 -Wall -Wextra -Werror, then link their contract ELF.
```

Result: exit code 0; `test_line_run.out` linked successfully.

Final offline build command compiled `syscfg_gen/ti_msp_dl_config.c` and every
`src/*.c` with the project TI Arm Clang flags, linked the line-run composition
contract, then linked `Clean_Car.out` against DriverLib.  Result: exit code 0.

Static checks run:

```powershell
.\tests\verify_control_params.ps1
.\tests\verify_line_run.ps1
.\tests\verify_project_metadata.ps1
git diff --check
```

Result: all listed checks passed; whitespace check was clean.

## Self-review

- The public tuning reset intentionally leaves `gRunTicks` unchanged, so a
  gain update cannot extend the existing 10-second run budget.
- The telemetry snapshot is issued after `Motor_SetSpeed()` and captures the
  same encoder readings and PWM values used for that motor command.
- The test compiles the DriverLib-free protocol source into its separate
  composition ELF because the existing line-run test link command intentionally
  links only `test_line_run.o` and `line_run.o`.
- No generated SysConfig file was edited or regenerated, and the user-owned
  `empty.syscfg` modification was left untouched.

## Issue retained outside this task

`tests/verify_control_rate.ps1` fails before Task 2 verification with
`控制周期必须为 6.667 ms。` because the pre-existing user modification to
`empty.syscfg` no longer satisfies that unrelated static assertion.  It was
not changed or regenerated in this task.

## Hardware status

Only source, strict cross-compilation, linking, and static contracts were
verified.  The project was not flashed and no vehicle behavior was tested.
