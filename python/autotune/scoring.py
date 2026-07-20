# ----- AI
"""
可复算试验评分。

评分只依赖已保存的 TrialSummary 和 ScoreConfig，不依赖实时遥测到达率。
分数越低越好；安全故障惩罚独立且大于任何正常项差异。
"""

from dataclasses import asdict, dataclass
import math

from .models import TrialSummary


@dataclass(frozen=True, slots=True)
class ScoreConfig:
    """评分权重、归一化上限和失败惩罚。"""

    weight_mae: float
    weight_rmse: float
    weight_p95: float
    weight_lost: float
    weight_saturation: float
    weight_oscillation: float
    weight_speed: float
    error_scale: float
    speed_scale: float
    failure_penalty: float

    @classmethod
    def defaults(cls) -> "ScoreConfig":
        return cls(2.0, 2.0, 1.0, 5.0, 2.0, 1.0, 1.0,
                   3500.0, 200.0, 1_000_000.0)

    def to_dict(self) -> dict[str, float]:
        """返回可与日志一起保存的稳定配置字典。"""

        return asdict(self)


@dataclass(frozen=True, slots=True)
class ScoreResult:
    """总分、失败标志和离线复算所需的各分项。"""

    total: float
    failed: bool
    components: dict[str, float]


def score_summary(summary: TrialSummary, config: ScoreConfig) -> ScoreResult:
    """按 `1-3` 公式计算越低越好的确定分数。"""

    samples = summary.sample_count
    failed = (
        samples <= 0
        or summary.fault != 0
        or summary.stop_reason != 2
        or summary.arithmetic_saturated != 0
        or summary.control_overruns != 0
    )
    divisor = max(samples, 1)
    mae = summary.abs_error_sum / divisor
    rmse = math.sqrt(max(summary.squared_error_sum / divisor, 0.0))
    p95 = float(summary.approximate_p95_error)
    lost_ratio = summary.lost_samples / divisor
    saturation_ratio = (
        summary.target_saturation_samples + summary.pwm_saturation_samples
    ) / (2.0 * divisor)
    oscillation_rate = summary.sign_flips / divisor
    mean_speed = (
        summary.left_speed_sum + summary.right_speed_sum
    ) / (2.0 * divisor)

    components = {
        "mae": mae,
        "rmse": rmse,
        "p95": p95,
        "lost_ratio": lost_ratio,
        "saturation_ratio": saturation_ratio,
        "oscillation_rate": oscillation_rate,
        "mean_speed": mean_speed,
        "failure_penalty": config.failure_penalty if failed else 0.0,
    }
    total = (
        config.weight_mae * (mae / config.error_scale)
        + config.weight_rmse * (rmse / config.error_scale)
        + config.weight_p95 * (p95 / config.error_scale)
        + config.weight_lost * lost_ratio
        + config.weight_saturation * saturation_ratio
        + config.weight_oscillation * oscillation_rate
        - config.weight_speed * (mean_speed / config.speed_scale)
        + components["failure_penalty"]
    )
    return ScoreResult(total=total, failed=failed, components=components)
# ----- AI
