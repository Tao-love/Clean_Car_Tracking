# ----- AI
"""C/Python 共用的参数、状态和汇总字节布局测试。"""

import unittest

from autotune.models import ControlParams, TrialSummary


class ControlParamsWireTests(unittest.TestCase):
    def test_round_trip_is_exactly_58_bytes(self) -> None:
        params = ControlParams(
            speed_kp_left_q16=1,
            speed_ki_left_q16=2,
            speed_feedforward_left_q16=3,
            speed_kp_right_q16=4,
            speed_ki_right_q16=5,
            speed_feedforward_right_q16=6,
            line_kp_q16=7,
            line_kd_q16=8,
            derivative_alpha_q16=32768,
            speed_integral_limit=10000,
            base_speed=10,
            max_target_speed=20,
            max_delta_speed=15,
            max_pwm=400,
            derivative_limit=3500,
            stall_pwm_threshold=300,
            stall_speed_threshold=1,
            telemetry_hz=10,
            control_overrun_limit=3,
        )

        payload = params.to_wire()

        self.assertEqual(len(payload), 58)
        self.assertEqual(ControlParams.from_wire(payload), params)

    def test_wire_decoder_rejects_wrong_length(self) -> None:
        with self.assertRaises(ValueError):
            ControlParams.from_wire(bytes(57))

    def test_safe_default_cannot_move_the_car(self) -> None:
        params = ControlParams.safe_defaults()
        self.assertEqual(params.base_speed, 0)
        self.assertLessEqual(params.max_pwm, 400)


class TrialSummaryWireTests(unittest.TestCase):
    def test_decodes_signed_64_bit_accumulators(self) -> None:
        values = [
            2, 500, 500, 2, 0, 0, 3, 3, 4, 5, 6, 7, 3500, 3000,
            400, 399, 12345, -23456,
            100, 200, -300, 400, -500, 600, 700, 800, 900, 1000,
        ]
        payload = TrialSummary.pack_test_vector(values)

        summary = TrialSummary.from_wire(payload)

        self.assertEqual(len(payload), 118)
        self.assertEqual(summary.param_version, 2)
        self.assertEqual(summary.sample_count, 500)
        self.assertEqual(summary.right_encoder_counts, -23456)
        self.assertEqual(summary.left_target_sum, -300)
        self.assertEqual(summary.right_abs_pwm_sum, 800)


if __name__ == "__main__":
    unittest.main()
# ----- AI
