# UART1 Raw Echo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make UART1 return each received byte unchanged so the battery-powered car can be verified from Python on the CH340 COM14 link.

**Architecture:** Add a small hardware-independent echo poller that accepts the existing UART transport read/write functions and handles a bounded number of bytes. `main.c` will initialize only the existing UART transport and invoke that poller instead of the binary protocol parser, leaving SysConfig, motor control, safety, timer, and queue ownership unchanged.

**Tech Stack:** C17-compatible TI Arm Clang project, MSPM0 DriverLib/SysConfig, existing UART interrupt transport, existing ARM cross-compiled contract test, Python 3 + pyserial.

## Global Constraints

- Keep UART1 on PA8 TX and PA9 RX at 9600 8N1; do not edit generated SysConfig output.
- Keep the current directory structure and all unrelated motor, safety, sensor, key, Flash, and build configuration unchanged.
- The raw echo consumes at most 64 bytes per main-loop pass and uses the existing non-blocking UART queues.
- Do not initiate a motor trial or transmit periodic data as part of echo mode.
- Hardware validation uses a battery-powered target and a CH340 connected only by TX, RX, and GND; do not connect either adapter power pin.

---

### Task 1: Add a tested, bounded UART echo poller

**Files:**
- Create: `inc/uart_echo.h`
- Create: `src/uart_echo.c`
- Modify: `tests/firmware_host/test_core.c`
- Modify: `tools/verify.ps1`

**Interfaces:**
- Consumes: `bool (*readByte)(uint8_t *value)` and `bool (*writeBytes)(const uint8_t *data, uint16_t length, bool highPriority)`.
- Produces: `uint16_t UART_Echo_Poll(UART_EchoReadByte readByte, UART_EchoWriteBytes writeBytes, uint16_t byteBudget)`; its return value is the number of consumed RX bytes.
- Invariants: each consumed byte is submitted once to `writeBytes` as a one-byte high-priority TX frame; a rejected write does not block later RX bytes within the budget.

- [ ] **Step 1: Write the failing contract test**

Add `#include "uart_echo.h"` to `tests/firmware_host/test_core.c`. Add fake read/write callbacks over a three-byte input and this test before `main`:

```c
static int Test_UartEcho(void)
{
    static const uint8_t input[] = { 'p', 'i', 'n' };
    uint16_t consumed;

    TestEcho_Reset(input, sizeof(input));
    consumed = UART_Echo_Poll(TestEcho_Read, TestEcho_Write, 2U);
    if ((consumed != 2U) || (gEchoOutputLength != 2U) ||
        (gEchoOutput[0] != 'p') || (gEchoOutput[1] != 'i')) {
        return 55;
    }
    consumed = UART_Echo_Poll(TestEcho_Read, TestEcho_Write, 64U);
    if ((consumed != 1U) || (gEchoOutputLength != 3U) ||
        (gEchoOutput[2] != 'n') || !gEchoWritesAreOneByteHighPriority) {
        return 56;
    }
    return 0;
}
```

Call `Test_UartEcho()` after `Test_RingBuffer()` in `main`. Supply `TestEcho_Reset`, `TestEcho_Read`, and `TestEcho_Write` in the same test file; `TestEcho_Write` must append exactly one byte and record whether `length == 1U` and `highPriority` are true.

- [ ] **Step 2: Run the test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1`

Expected: compilation fails because `uart_echo.h` and `UART_Echo_Poll` do not exist. Do not add production code before observing this failure.

- [ ] **Step 3: Write the minimal echo interface and implementation**

Create `inc/uart_echo.h`:

```c
#ifndef UART_ECHO_H
#define UART_ECHO_H

#include <stdbool.h>
#include <stdint.h>

typedef bool (*UART_EchoReadByte)(uint8_t *value);
typedef bool (*UART_EchoWriteBytes)(
    const uint8_t *data, uint16_t length, bool highPriority);

uint16_t UART_Echo_Poll(UART_EchoReadByte readByte,
    UART_EchoWriteBytes writeBytes, uint16_t byteBudget);

#endif /* UART_ECHO_H */
```

Create `src/uart_echo.c`:

```c
#include <stdint.h>

#include "uart_echo.h"

uint16_t UART_Echo_Poll(UART_EchoReadByte readByte,
    UART_EchoWriteBytes writeBytes, uint16_t byteBudget)
{
    uint16_t consumed = 0U;
    uint8_t value;

    if ((readByte == 0) || (writeBytes == 0)) {
        return 0U;
    }
    while ((consumed < byteBudget) && readByte(&value)) {
        (void) writeBytes(&value, 1U, true);
        consumed++;
    }
    return consumed;
}
```

Append `(Join-Path $Output 'uart_echo.o')` to `$ContractObjects` in `tools/verify.ps1` so the contract ELF links the implementation under test.

- [ ] **Step 4: Run the contract/build verification to verify it passes**

Run: `powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1`

Expected: exit code 0 and `验证通过: ...\.build\verify\USB_nobluetooth.out`. SysConfig may print the two existing STOP/STANDBY retention informational notices; they are not errors.

- [ ] **Step 5: Commit the tested poller**

```powershell
git add inc/uart_echo.h src/uart_echo.c tests/firmware_host/test_core.c tools/verify.ps1
git commit -m "feat: add bounded UART raw echo poller"
```

### Task 2: Switch the application loop to raw echo mode

**Files:**
- Modify: `src/main.c`
- Modify: `README.md`

**Interfaces:**
- Consumes: `UART_Echo_Poll`, `UART_Transport_ReadByte`, and `UART_Transport_Write`.
- Produces: a main loop that echoes up to `UART_ECHO_BYTE_BUDGET` received bytes per pass.
- Removes from active runtime path: `Protocol_Init`, `Protocol_Poll`, `AppProtocol_Init`, and `App_CreateSessionId`; their source files remain unchanged and continue to build.

- [ ] **Step 1: Write the failing integration expectation**

In `tests/firmware_host/test_core.c`, add a second `Test_UartEcho` case where `TestEcho_Write` rejects its first write. Assert that `UART_Echo_Poll` still returns the complete input byte count and that later input bytes are offered to the writer. Return `57` if the behavior differs.

- [ ] **Step 2: Run the test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1`

Expected: the new rejection assertion fails with test return code 57 until the test fixture correctly observes rejected writes. The production poller must not be changed unless it violates the assertion.

- [ ] **Step 3: Integrate the already-tested poller in `main.c`**

Make these focused edits:

```c
#include "uart_echo.h"

#define UART_ECHO_BYTE_BUDGET (64U)
```

Remove `#include "app_protocol.h"`, `#include "protocol.h"`, the session-ID constants, and the `App_CreateSessionId` declaration/definition. After `UART_Transport_Init();`, do not initialize `AppProtocol` or `Protocol`. Replace the protocol poll in the main loop with:

```c
(void) UART_Echo_Poll(UART_Transport_ReadByte,
    UART_Transport_Write, UART_ECHO_BYTE_BUDGET);
```

Leave `SYSCFG_DL_init`, `Motor_Init`, `TrialManager_Init`, the KEY1 logic, and the timer ISR exactly as they are.

Update the README UART section to state that this temporary build uses `PA8/PA9`, `9600 8N1`, and raw byte-for-byte echo; document the three signal-only connections and state that it does not accept the previous binary protocol.

- [ ] **Step 4: Run all offline checks to verify green**

Run: `powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1`

Expected: exit code 0, all source files compile with `-Wall -Wextra -Werror`, the ARM contract test links, and the resulting firmware path is `.build\verify\USB_nobluetooth.out`.

- [ ] **Step 5: Commit the raw echo application mode**

```powershell
git add src/main.c README.md tests/firmware_host/test_core.c
git commit -m "feat: route UART1 through raw echo mode"
```

### Task 3: Flash and verify with the connected CH340

**Files:**
- Modify: none

**Interfaces:**
- Consumes: `.build\verify\USB_nobluetooth.out`, the existing XDS110 target configuration, and `COM14`.
- Produces: a recorded Python byte-for-byte echo result for a non-motor UART test frame.

- [ ] **Step 1: Prepare target safely**

Power the target from its battery, keep wheel drive safe (wheels suspended or motor supply disconnected), connect XDS110 SWDIO/SWCLK/GND and voltage reference as appropriate, and connect the CH340 only as `PA8 → RXD`, `PA9 → TXD`, `GND → GND`. Do not connect either CH340 power pin or an XDS110 3.3 V output to the battery-powered board.

- [ ] **Step 2: Flash the verified artifact**

In CCS, use `targetConfigs/MSPM0G3507.ccxml` with the identified XDS110 and program `.build\verify\USB_nobluetooth.out`. Reset/run the target after programming. Do not use CH340 UART BSL because this project has no approved UART-BSL target configuration.

- [ ] **Step 3: Run the Python echo check on COM14**

Close CCS serial monitors and run:

```powershell
@'
import serial

payload = b'echo-verify-20260724\n'
with serial.Serial('COM14', 9600, timeout=2) as port:
    port.reset_input_buffer()
    port.write(payload)
    port.flush()
    received = port.read(len(payload))
print(f'sent={payload!r}')
print(f'received={received!r}')
raise SystemExit(0 if received == payload else 1)
'@ | python -
```

Expected: `received=b'echo-verify-20260724\n'` and exit code 0. If the received byte sequence differs, stop and report the exact `received` bytes; do not start a motor trial or change UART baud/pins without a new diagnosis.

- [ ] **Step 4: Record the verification result**

Run `git status --short` and preserve only the intended source/documentation changes. Report the build output and the exact Python sent/received byte sequences.

## Plan self-review

- Spec coverage: Task 1 creates the bounded reusable echo behavior with a host contract test; Task 2 activates it without changing UART1 SysConfig or unrelated control code; Task 3 gives the precise COM14 hardware validation.
- Placeholder scan: no incomplete implementation steps or unspecified commands remain.
- Type consistency: `UART_Echo_Poll` accepts callbacks matching `UART_Transport_ReadByte` and `UART_Transport_Write`; the return type is `uint16_t` throughout.

