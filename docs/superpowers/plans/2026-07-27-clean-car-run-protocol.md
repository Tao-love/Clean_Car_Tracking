# Clean Car RUN Protocol Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Accept protected `RUN SEQ:<0..65535>*HH` commands over UART2 and start exactly one existing `LineRun` round only while idle.

**Architecture:** Keep parsing, checksum validation, ACK/ERR emission, and command dispatch inside `UART_Auto_PID.c`. After a validated RUN frame, call `LineRun_IsRunning()` and then the existing `LineRun_Start()` API; neither PWM nor safety/timeout logic gets a new path.

**Tech Stack:** C11-compatible TI Arm Clang firmware, existing UART2 polling service, host C contract tests, PowerShell source contracts.

## Global Constraints

- Preserve the user-owned `empty.syscfg` change and do not modify any `syscfg_gen/` file.
- RUN must use the existing ASCII XOR `*HH` protection and ACK/ERR response formatter.
- RUN never modifies tuning parameters, stops a round, bypasses `LineRun_Start()`, or changes the 1500-tick limit and safety protections.
- Host checks must cover valid idle RUN, invalid checksum, missing sequence, and busy RUN.

---

### Task 1: Define host-visible RUN expectations

**Files:**
- Modify: `tests/firmware_host/test_uart_auto_pid.c`

**Interfaces:**
- Consumes: `UART_Auto_PID_ProcessLineForTest(const char *)`.
- Produces: Counter-based evidence that a valid frame calls `LineRun_Start()` once only when `LineRun_IsRunning()` is false.

- [ ] **Step 1: Write the failing test**

Add `LineRun_IsRunning()` and `LineRun_Start()` test doubles with `gRunActive`, `gRunStartCalls`, and `gRunStartAllowed`. Add `Test_RunCommandRequiresProtectedIdleStart()` that frames `RUN SEQ:18`, expects one successful call while idle, and then verifies bad checksum, `RUN*HH`, and a busy valid RUN neither start nor increment the counter.

- [ ] **Step 2: Run the test to verify it fails**

Run: `gmake -C Debug all`

Expected: host UART contract compilation/linking fails because the production command parser does not yet recognize RUN (or the test returns its RUN-specific failure value).

### Task 2: Dispatch protected RUN through LineRun

**Files:**
- Modify: `src/UART_Auto_PID.c`
- Modify: `inc/UART_Auto_PID.h`

**Interfaces:**
- Consumes: `bool LineRun_IsRunning(void)` and `bool LineRun_Start(void)` from `line_run.h`.
- Produces: `ACK,<seq>*HH` for a valid idle RUN; `ERR,<seq>,BUSY*HH`, `ERR,<seq>,CHECKSUM*HH`, `ERR,<seq>,FORMAT*HH`, or `ERR,<seq>,STATE*HH` for rejected RUN frames.

- [ ] **Step 1: Keep command detection explicit**

Recognize only `RUN` followed by whitespace, `*`, or end of input. Reuse the existing XOR delimiter and hexadecimal decoder. Parse exactly one `SEQ:<0..65535>` RUN field, allowing surrounding ASCII spaces but rejecting absent, duplicate, malformed, or trailing non-space fields.

- [ ] **Step 2: Implement the smallest idle-only path**

After checksum and RUN syntax succeed, check `LineRun_IsRunning()`. If true, set reason `BUSY`; otherwise call `LineRun_Start()` once. Return accepted only when it returns true; otherwise set reason `STATE`.

- [ ] **Step 3: Preserve SETALL behavior**

Leave the existing SETALL candidate-copy, range validation, atomic replacement, and reset callback unchanged. Keep `STATUS` read-only and unknown commands ignored.

### Task 3: Verify integration without hardware actuation

**Files:**
- Modify: `tests/verify_uart2_syscfg.ps1`

- [ ] **Step 1: Extend the source contract**

Require `RUN`, `LineRun_IsRunning`, and `LineRun_Start` tokens in the UART runtime source while retaining the non-blocking UART, main-loop-only service, 21-column telemetry, and no-UART-in-control-tick checks.

- [ ] **Step 2: Run complete offline verification**

Run: `gmake -C Debug all`, `tests/verify_uart2_syscfg.ps1`, `tests/verify_line_run.ps1`, and `tools/verify.ps1`.

Expected: all source/build contracts pass. Do not flash or run the motor; the first hardware protocol check remains a user-performed wheels-suspended test.

## Self-review

- Spec coverage: valid RUN, XOR, sequence parsing, ACK/ERR dispatch, idle-only start, BUSY/FORMAT/CHECKSUM/STATE rejection, and preserved LineRun safety all map to Tasks 1-3.
- Placeholder scan: no unfinished markers or unspecified implementation behavior remains.
- Interface check: all production calls are existing `line_run.h` APIs; no new SysConfig or motor API is introduced.
