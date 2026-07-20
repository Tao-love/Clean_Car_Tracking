# ----- AI
"""ACK/STATUS/TELEMETRY 等固定业务 payload 的小端解码。"""

from dataclasses import dataclass
import struct


@dataclass(frozen=True, slots=True)
class Ack:
    acknowledged_type: int
    status: int
    param_version: int
    session_id: int

    @classmethod
    def from_wire(cls, payload: bytes) -> "Ack":
        if len(payload) != 8:
            raise ValueError("ACK/NACK payload 必须为 8 字节")
        message_type, status, version, session = struct.unpack("<BBHI", payload)
        return cls(message_type, status, version, session)


@dataclass(frozen=True, slots=True)
class Status:
    state: int
    fault: int
    stop_reason: int
    summary_ready: bool
    param_version: int
    trial_ticks: int
    control_tick: int
    control_overruns: int
    session_id: int

    @classmethod
    def from_wire(cls, payload: bytes) -> "Status":
        if len(payload) != 20:
            raise ValueError("STATUS payload 必须为 20 字节")
        values = struct.unpack("<4B2H3I", payload)
        return cls(*values[:3], bool(values[3]), *values[4:])


@dataclass(frozen=True, slots=True)
class Telemetry:
    tick: int
    bitmap: int
    state: int
    fault: int
    line_valid: bool
    error: int
    left_target: int
    right_target: int
    left_speed: int
    right_speed: int
    left_pwm: int
    right_pwm: int
    param_version: int

    @classmethod
    def from_wire(cls, payload: bytes) -> "Telemetry":
        if len(payload) != 24:
            raise ValueError("TELEMETRY payload 必须为 24 字节")
        tick, bitmap, state, fault, valid, *rest = struct.unpack(
            "<I4B7hH", payload
        )
        return cls(tick, bitmap, state, fault, bool(valid), *rest)
# ----- AI
