# UART2 startup beacon design

## Goal

Add a one-time UART2 startup beacon to separate the receive-path echo diagnosis from firmware execution and TX2/PB15 hardware verification.

## Behavior

Immediately after the existing UART transport is initialized, the firmware queues the exact high-priority byte sequence `UART2 READY\r\n`.  It is sent once per reset through the existing non-blocking TX queue.  The raw byte-for-byte echo loop remains active after the beacon.

## Scope and safety

Only `src/main.c`, the host contract test, and UART documentation change.  The UART2 PB15/PB16 SysConfig configuration, motor/safety code, timer, and GPIO ownership stay unchanged.  The message does not start a trial or alter PWM; the target remains battery powered and the CH340 stays signal-only on the PCB USART2 connector.

## Validation

The host contract test verifies the banner byte sequence and high-priority one-time enqueue request.  After XDS110 programming and reset/run, Python on COM14 at 9600 8N1 must receive `b'UART2 READY\r\n'` before any test byte is sent.  A missing banner means the fault is independent of CH340-to-RX2 input.
