# ----- AI
"""
与 MCU 共享的定长数据模型。

本模块只处理类型和小端序列化，不打开串口、不修改试验状态。
增益为有符号 Q16.16，速度为每 10 ms 编码器计数，PWM 是 0..1600 比较值。
"""

from dataclasses import dataclass
import struct
from typing import ClassVar, Iterable


@dataclass(frozen=True, slots=True)
class ControlParams:
    """对应 C `ControlParams` 的 58 字节稳定协议布局。"""

    speed_kp_left_q16: int
    speed_ki_left_q16: int
    speed_feedforward_left_q16: int
    speed_kp_right_q16: int
    speed_ki_right_q16: int
    speed_feedforward_right_q16: int
    line_kp_q16: int
    line_kd_q16: int
    derivative_alpha_q16: int
    speed_integral_limit: int
    base_speed: int
    max_target_speed: int
    max_delta_speed: int
    max_pwm: int
    derivative_limit: int
    stall_pwm_threshold: int
    stall_speed_threshold: int
    telemetry_hz: int
    control_overrun_limit: int

    _STRUCT: ClassVar[struct.Struct] = struct.Struct("<10i7h2H")

    @classmethod
    def safe_defaults(cls) -> "ControlParams":
        """返回与 MCU 一致且 base_speed=0 的不动车默认值。"""

        return cls(
            0, 0, 0, 0, 0, 0, 0, 0, 32768, 10000,
            0, 100, 100, 400, 3500, 300, 1, 10, 3,
        )

    def to_wire(self) -> bytes:
        """编码为 MCU `SET_PARAMS` 中 session ID 之后的 58 字节。"""

        return self._STRUCT.pack(*self.as_tuple())

    @classmethod
    def from_wire(cls, payload: bytes) -> "ControlParams":
        """从严格 58 字节小端载荷解码，长度错立即拒绝。"""

        if len(payload) != cls._STRUCT.size:
            raise ValueError(f"ControlParams payload 必须为 {cls._STRUCT.size} 字节")
        return cls(*cls._STRUCT.unpack(payload))

    def as_tuple(self) -> tuple[int, ...]:
        """按 C 协议字段顺序返回整数元组。"""

        return (
            self.speed_kp_left_q16,
            self.speed_ki_left_q16,
            self.speed_feedforward_left_q16,
            self.speed_kp_right_q16,
            self.speed_ki_right_q16,
            self.speed_feedforward_right_q16,
            self.line_kp_q16,
            self.line_kd_q16,
            self.derivative_alpha_q16,
            self.speed_integral_limit,
            self.base_speed,
            self.max_target_speed,
            self.max_delta_speed,
            self.max_pwm,
            self.derivative_limit,
            self.stall_pwm_threshold,
            self.stall_speed_threshold,
            self.telemetry_hz,
            self.control_overrun_limit,
        )


@dataclass(frozen=True, slots=True)
class TrialSummary:
    """MCU 本地累加并作为最终评分依据的 102 字节汇总。"""

    param_version: int
    sample_count: int
    run_ticks: int
    stop_reason: int
    fault: int
    arithmetic_saturated: int
    lost_samples: int
    longest_lost_ticks: int
    target_saturation_samples: int
    pwm_saturation_samples: int
    sign_flips: int
    control_overruns: int
    max_abs_error: int
    approximate_p95_error: int
    max_abs_left_pwm: int
    max_abs_right_pwm: int
    left_encoder_counts: int
    right_encoder_counts: int
    abs_error_sum: int
    squared_error_sum: int
    left_target_sum: int
    right_target_sum: int
    left_speed_sum: int
    right_speed_sum: int
    left_abs_pwm_sum: int
    right_abs_pwm_sum: int

    _STRUCT: ClassVar[struct.Struct] = struct.Struct("<3H2B7H4h2i8q")

    @classmethod
    def from_wire(cls, payload: bytes) -> "TrialSummary":
        """解码严格 102 字节汇总。"""

        if len(payload) != cls._STRUCT.size:
            raise ValueError(f"TRIAL_SUMMARY payload 必须为 {cls._STRUCT.size} 字节")
        return cls(*cls._STRUCT.unpack(payload))

    @classmethod
    def pack_test_vector(cls, values: Iterable[int]) -> bytes:
        """仅供 C/Python 黄金向量测试按稳定布局生成字节。"""

        return cls._STRUCT.pack(*tuple(values))
# ----- AI
