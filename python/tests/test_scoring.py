# ----- AI
"""评分必须可复算，且任何安全失败不能被速度奖励抵消。"""

import unittest

from autotune.models import TrialSummary
from autotune.scoring import ScoreConfig, score_summary


def make_summary(**changes: int) -> TrialSummary:
    values = dict(
        param_version=1, sample_count=500, run_ticks=500,
        stop_reason=2, fault=0, arithmetic_saturated=0,
        lost_samples=0, longest_lost_ticks=0,
        target_saturation_samples=0, pwm_saturation_samples=0,
        sign_flips=0, control_overruns=0, special_pattern_samples=0,
        max_abs_error=500,
        approximate_p95_error=500, max_abs_left_pwm=200,
        max_abs_right_pwm=200, max_abs_left_target=20,
        max_abs_right_target=20, max_abs_left_speed=19,
        max_abs_right_speed=19, left_encoder_counts=1000,
        right_encoder_counts=1000, abs_error_sum=50000,
        squared_error_sum=10000000, left_target_sum=10000,
        right_target_sum=10000, left_speed_sum=9000,
        right_speed_sum=9000, left_abs_pwm_sum=50000,
        right_abs_pwm_sum=50000, speed_abs_error_sum=1000,
        speed_squared_error_sum=10000,
    )
    values.update(changes)
    return TrialSummary(**values)


class ScoringTests(unittest.TestCase):
    def test_same_summary_and_config_produce_identical_score(self) -> None:
        config = ScoreConfig.defaults()
        summary = make_summary()
        self.assertEqual(score_summary(summary, config), score_summary(summary, config))

    def test_fault_penalty_dominates_maximum_speed_reward(self) -> None:
        config = ScoreConfig.defaults()
        safe = score_summary(make_summary(), config)
        failed = score_summary(
            make_summary(fault=1, stop_reason=4, left_speed_sum=32767 * 500,
                         right_speed_sum=32767 * 500),
            config,
        )
        self.assertGreater(failed.total, safe.total + 100_000)

    def test_zero_samples_is_always_failure(self) -> None:
        result = score_summary(make_summary(sample_count=0, run_ticks=0), ScoreConfig.defaults())
        self.assertTrue(result.failed)


if __name__ == "__main__":
    unittest.main()
# ----- AI
