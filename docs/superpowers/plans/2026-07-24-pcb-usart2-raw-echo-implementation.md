# PCB USART2 Raw Echo Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Route the existing raw UART echo through the PCB USART2 connector on UART2 PB15/PB16.

**Architecture:** Keep the UART_DEBUG instance name and transport source unchanged. Change its SysConfig peripheral assignment and pinmux to UART2/PB15/PB16, let SysConfig regenerate the matching macros and IRQ binding, and prove the generated configuration with a small PowerShell contract test.

**Tech Stack:** MSPM0 SysConfig, TI Arm Clang verification script, PowerShell, Python 3 + pyserial.

## Global Constraints

- Change only editable empty.syscfg, runtime documentation, and the new SysConfig contract test; do not hand-edit generated output.
- Preserve 9600 8N1, UART RX interrupt, one-entry RX FIFO threshold, raw echo behavior, and all non-UART pin assignments.
- Use PCB USART2 pin 1 RX2/PB16, pin 2 TX2/PB15, pin 3 GND, and leave pin 4 5V disconnected.
- The target remains battery powered and the CH340 uses signal-only wiring.

---

### Task 1: Verify and migrate the UART SysConfig source

**Files:**
- Create: tests/verify_uart2_syscfg.ps1
- Modify: empty.syscfg lines 184-191
- Modify: README.md UART wiring section

**Interfaces:**
- Consumes: syscfg_gen/ti_msp_dl_config.h.
- Produces: a zero-exit contract confirming UART2, UART2_IRQHandler, GPIOB, PB15 TX and PB16 RX macros; generated UART_DEBUG macros remain the transport interface.
- Hardware mapping: PCB USART2 pin 2 is MCU TX2/PB15 and pin 1 is MCU RX2/PB16.

- [ ] **Step 1: Write the failing generated-configuration contract**

Create tests/verify_uart2_syscfg.ps1:

~~~powershell
param(
    [string]$HeaderPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'syscfg_gen\ti_msp_dl_config.h')
)

$required = @(
    '#define UART_DEBUG_INST                                                    UART2',
    '#define UART_DEBUG_INST_IRQHandler                              UART2_IRQHandler',
    '#define GPIO_UART_DEBUG_RX_PORT                                            GPIOB',
    '#define GPIO_UART_DEBUG_TX_PORT                                            GPIOB',
    '#define GPIO_UART_DEBUG_RX_PIN                                    DL_GPIO_PIN_16',
    '#define GPIO_UART_DEBUG_TX_PIN                                    DL_GPIO_PIN_15',
    '#define GPIO_UART_DEBUG_IOMUX_RX_FUNC                 IOMUX_PINCM33_PF_UART2_RX',
    '#define GPIO_UART_DEBUG_IOMUX_TX_FUNC                 IOMUX_PINCM32_PF_UART2_TX',
    '#define UART_DEBUG_BAUD_RATE                                              (9600)'
)
$text = Get-Content -Raw -LiteralPath $HeaderPath
$missing = $required | Where-Object { -not $text.Contains($_) }
if ($missing.Count -ne 0) {
    $missing | ForEach-Object { Write-Error "Missing: $_" }
    exit 1
}
Write-Host 'UART2 PB15/PB16 SysConfig contract passed.'
~~~

- [ ] **Step 2: Run the contract to verify it fails on the current UART1 configuration**

Run: powershell -ExecutionPolicy Bypass -File .\tests\verify_uart2_syscfg.ps1

Expected: exit code 1 and missing UART2/PB15/PB16 macro messages, because the existing generated header declares UART1/PA8/PA9.

- [ ] **Step 3: Make the minimum SysConfig and documentation changes**

In empty.syscfg, keep the existing UART_DEBUG settings and only replace:

~~~js
UART_DEBUG.peripheral.$assign = "UART2";
UART_DEBUG.peripheral.txPin.$assign = "PB15";
UART_DEBUG.peripheral.rxPin.$assign = "PB16";
~~~

Update README wiring to identify the PCB USART2 connector:

~~~text
PCB USART2 pin 2 (TX2/PB15) -> USB-TTL RXD
PCB USART2 pin 1 (RX2/PB16) -> USB-TTL TXD
PCB USART2 pin 3 (GND)      -> USB-TTL GND
PCB USART2 pin 4 (5V)       -> not connected
~~~

- [ ] **Step 4: Regenerate and verify the exact UART2 contract**

Run:

~~~powershell
powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1
powershell -ExecutionPolicy Bypass -File .\tests\verify_uart2_syscfg.ps1
~~~

Expected: both commands exit 0. The build emits no compiler warnings or errors; SysConfig may print only its existing PWM/TRNG STOP/STANDBY informational notices.

- [ ] **Step 5: Commit source, test, and documentation**

~~~powershell
git add empty.syscfg README.md tests/verify_uart2_syscfg.ps1
git commit -m "feat: route raw echo to PCB USART2"
~~~

### Task 2: Flash and run the CH340 echo test

**Files:**
- Modify: none

**Interfaces:**
- Consumes: .build\verify\USB_nobluetooth.out and CH340 COM14 at 9600 8N1.
- Produces: a byte-for-byte Python echo result.

- [ ] **Step 1: Wire and flash safely**

With the target battery-powered and motor drive safe, wire CH340 RXD to PCB USART2 pin 2, TXD to pin 1, and GND to pin 3. Leave the header 5V pin and both CH340 power pins unconnected. Flash .build\verify\USB_nobluetooth.out through XDS110 and reset/run.

- [ ] **Step 2: Run the byte-for-byte test**

Run:

~~~powershell
@'
import serial

payload = b'echo-usart2-pb15-pb16\n'
with serial.Serial('COM14', 9600, timeout=2) as port:
    port.reset_input_buffer()
    port.write(payload)
    port.flush()
    received = port.read(len(payload))
print('sent=', repr(payload))
print('received=', repr(received))
raise SystemExit(0 if received == payload else 1)
'@ | python -
~~~

Expected: exit code 0 and identical sent/received byte sequences. On mismatch, report the exact received bytes and CH340 TXD/RXD indicator behavior before changing firmware or wiring.

## Plan self-review

- Spec coverage: Task 1 moves only the SysConfig source of truth and asserts all generated UART2 PB15/PB16 properties; Task 2 tests the exact PCB connector and COM14 behavior.
- Placeholder scan: no incomplete steps or unspecified pin assignments remain.
- Type consistency: no application-level interface changes; generated UART_DEBUG macros preserve the transport interface.

