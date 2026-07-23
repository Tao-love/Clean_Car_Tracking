# UART1 raw echo design

## Goal

Temporarily make the existing UART1 link return every received byte unchanged so a USB-TTL adapter on COM14 can be checked with a Python serial test.

## Scope

- Keep the existing UART1 SysConfig assignment: UART1, PA8 TX, PA9 RX, 9600 8N1.
- Reuse the existing interrupt-driven UART transport and its bounded RX/TX queues.
- Replace only the main-loop protocol polling path with a bounded raw-echo poll.
- Do not change motor, safety, timing, pin assignments, project layout, or generated SysConfig files.

## Runtime behavior

On each main-loop pass, the firmware removes at most 64 bytes from the UART RX queue and attempts to enqueue each byte for high-priority TX.  No protocol parser, command handler, telemetry, or periodic UART output runs.  If TX queue capacity is unavailable, the byte is dropped; the existing transport rejection counter records that condition and the 100 Hz control loop remains non-blocking.

## Validation

Build with the existing CCS Debug configuration, then run a Python/pyserial check on COM14 at 9600 8N1.  Send a unique line such as `echo-verify-20260724\n` and require an identical received byte sequence.  The target is powered by its battery; USB-TTL supplies signals and common ground only.
