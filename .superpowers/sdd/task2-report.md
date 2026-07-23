# Task 2 report

## Files changed

- Deleted the eight Bluetooth-only project documents listed in Task 2.
- Rewrote `README.md` as a current-firmware entry point: KEY1 wheel-speed trial (6/6), source parameter location, UART1 binary-protocol boundary, XDS110 target, and USB-TTL PA8/PA9/GND wiring without a 5 V connection.
- Updated the UART1 comment in `empty.syscfg` without changing its generated configuration inputs.
- Generalized Bluetooth/JDY wording in the manual-tuning and historical USB BSL design/plan documents.

## Checks run

- Read the Task 2 plan and inspected `git status`, current README, SysConfig source, preserved documents, `src/main.c`, `src/control_params.c`, and `targetConfigs/MSPM0G3507.ccxml`.
- No build, test, flashing, SysConfig generation, or validation script was run, per task constraints.

## Concerns

- The preserved USB UART BSL documents are historical cross-project records. They retain their historical BSL configuration claims; only Bluetooth/JDY terminology was generalized.
- The active target configuration in this worktree is `targetConfigs/MSPM0G3507.ccxml` using XDS110, as stated in the rewritten README.
