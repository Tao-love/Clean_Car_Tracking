# ----- AI
"""
`1-3` 四阶段自动整定编排。

阶段 1/2 必须车轮悬空；阶段 3/4 必须在低速赛道上进行。
所有候选均调用 MCU 本地最多 5 秒试验，本模块不能绕过参数或 PWM 硬边界。
"""

from dataclasses import dataclass, replace
import itertools
from collections.abc import Iterable

import numpy as np

from .models import ControlParams, TrialSummary
from .optimizer import BoundedCoordinateSearch, Evaluation, SearchResult
from .trial import TrialResult, TrialRunner


@dataclass(frozen=True, slots=True)
class MotorIdentificationPoint:
    pwm: int
    mean_left_speed: float
    mean_right_speed: float
    failed: bool


class AutotuneStages:
    """电机辨识→速度 PI→低速循线 PD→基础速度爬升。"""

    def __init__(self, runner: TrialRunner) -> None:
        self.runner = runner

    def identify_motors(
        self, params: ControlParams, pwm_levels: Iterable[int]
    ) -> list[MotorIdentificationPoint]:
        """悬空对左右电机同时做受限开环 PWM 阶梯。"""

        points: list[MotorIdentificationPoint] = []
        for pwm in sorted(set(map(int, pwm_levels))):
            if not 0 <= pwm <= params.max_pwm:
                raise ValueError(f"PWM {pwm} 超出 0..{params.max_pwm}")
            result = self.runner.run_candidate(
                params, mode=2, left_command=pwm, right_command=pwm
            )
            divisor = max(result.summary.sample_count, 1)
            points.append(
                MotorIdentificationPoint(
                    pwm=pwm,
                    mean_left_speed=result.summary.left_speed_sum / divisor,
                    mean_right_speed=result.summary.right_speed_sum / divisor,
                    failed=result.score.failed,
                )
            )
            if result.score.failed:
                break
        return points

    def tune_speed_pi(
        self,
        initial: ControlParams,
        target_speed: int,
        fields: dict[str, tuple[int, int, int]],
        max_rounds: int = 3,
    ) -> SearchResult:
        """悬空使用左右相同速度阶跃和速度误差累计调 PI/前馈。"""

        if not 0 < target_speed <= initial.max_target_speed:
            raise ValueError("target_speed 超出当前参数上限")

        def evaluate(candidate: ControlParams) -> Evaluation:
            result = self.runner.run_candidate(
                candidate,
                mode=1,
                left_command=target_speed,
                right_command=target_speed,
            )
            samples = max(result.summary.sample_count, 1)
            tracking = result.summary.speed_abs_error_sum / (2.0 * samples)
            saturation = result.summary.pwm_saturation_samples / samples
            score = tracking + 100.0 * saturation
            if result.score.failed:
                score += 1_000_000.0
            return Evaluation(score, result.score.failed)

        return BoundedCoordinateSearch(fields, max_rounds).run(initial, evaluate)

    def tune_line_pd(
        self,
        initial: ControlParams,
        fields: dict[str, tuple[int, int, int]],
        max_rounds: int = 3,
    ) -> SearchResult:
        """在低速赛道上按标准循线分数搜索 line Kp/Kd。"""

        def evaluate(candidate: ControlParams) -> Evaluation:
            result = self.runner.run_candidate(candidate, mode=0)
            return Evaluation(result.score.total, result.score.failed)

        return BoundedCoordinateSearch(fields, max_rounds).run(initial, evaluate)

    def climb_speed(
        self, initial: ControlParams, speeds: Iterable[int]
    ) -> list[TrialResult]:
        """只从已验证参数向上逐级增加 base_speed，首个失败即停止。"""

        results: list[TrialResult] = []
        for speed in sorted(set(map(int, speeds))):
            if not 0 <= speed <= initial.max_target_speed:
                raise ValueError(f"base_speed {speed} 越界")
            result = self.runner.run_candidate(replace(initial, base_speed=speed))
            results.append(result)
            if result.score.failed:
                break
        return results

    def grid_search(
        self,
        initial: ControlParams,
        field_values: dict[str, Iterable[int]],
    ) -> TrialResult:
        """对少量参数执行确定网格搜索，返回最低安全分候选。"""

        names = list(field_values)
        value_lists = [list(map(int, field_values[name])) for name in names]
        if any(not values for values in value_lists):
            raise ValueError("网格的每个字段至少需要一个值")
        results: list[TrialResult] = []
        for values in itertools.product(*value_lists):
            candidate = replace(initial, **dict(zip(names, values)))
            result = self.runner.run_candidate(candidate)
            if not result.score.failed:
                results.append(result)
        if not results:
            raise RuntimeError("网格中没有任何安全候选")
        scores = np.asarray([item.score.total for item in results], dtype=float)
        return results[int(np.argmin(scores))]
# ----- AI
