# Right Encoder Sign Calibration Design

## Goal

Make the right encoder report a positive speed when the right motor is driven in the configured forward direction, so both wheel feedback values have the same sign as the positive speed targets.

## Evidence

The suspended-wheel telemetry reports `left_target=27`, `left_speed=+8`, `right_target=27`, and `right_speed=-8` while both PWM outputs are `+300`.  The opposite right feedback sign makes the host's aggregate speed appear as zero and makes the right speed PI see a larger positive error despite the wheel turning.

## Selected Approach

Set the existing compile-time calibration constant `ENCODER_RIGHT_REVERSE` in `src/encoder.c` from `0` to `1`.  `Encoder_HandleRightEdge()` already applies this constant after decoding the A/B phase relationship, so the change negates only the right encoder step before it reaches the count, raw-speed, and filtered-speed paths.

## Alternatives Rejected

- Swap the right encoder A/B wires: this can also reverse the sign, but changes physical wiring and is unnecessary because the firmware provides a per-wheel calibration constant.
- Reverse the right motor direction or modify the speed PID: these would hide or compound a feedback-sign error and do not make the measured direction match the forward target convention.

## Boundary

Only `src/encoder.c` changes.  The motor PWM direction, PID gains, feedforward, `maxPwm`, SysConfig source (`empty.syscfg`), generated SysConfig files, serial protocol, and all protection behavior remain unchanged.

## Verification

Add a source-contract host test that requires the right calibration constant to be enabled and that confirms the existing right-edge handler applies it to the decoded step.  Run that test before and after the one-line change.  The user then compiles and manually burns the firmware; with the wheels suspended and a positive target, both displayed wheel speeds must be positive.  No automatic flashing is performed.
