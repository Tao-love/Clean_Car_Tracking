# PCB USART2 raw echo migration design

## Goal

Move the temporary byte-for-byte echo link from UART1 to the PCB's dedicated `USART2` connector so the CH340 test adapter uses the board-routed UART2 signals.

## Confirmed hardware mapping

The supplied schematic labels the four-pin `USART2` connector as:

- Pin 1: `RX2`, connected to MSPM0 `PB16` / UART2_RX.
- Pin 2: `TX2`, connected to MSPM0 `PB15` / UART2_TX.
- Pin 3: GND.
- Pin 4: 5 V; unused because the car is battery powered.

The CH340 connection is crossed: `RXD` to connector pin 2, `TXD` to connector pin 1, and GND to connector pin 3.  Neither CH340 power pin connects to the PCB.

## Firmware design

Change only the editable SysConfig UART instance from UART1/PA8/PA9 to UART2/PB15/PB16, retaining 9600 8N1, the one-byte RX FIFO threshold, and RX interrupt.  The existing transport consumes generated `UART_DEBUG_*` macros, so regeneration updates its peripheral instance and IRQ without a transport rewrite.  The existing bounded raw-echo poller remains the active application behavior.

## Exclusions and safety

Do not reuse PA10/PA11, which are assigned to the ultrasonic module, and do not alter motor, sensor, key, timer, Flash, or other pin assignments.  Flash with XDS110 while the battery powers the target; the CH340 is signal-only.  Validate at 9600 8N1 with Python on COM14 by requiring the received bytes to exactly equal the sent payload.
