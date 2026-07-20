# ----- AI
"""
单次 5 秒候选参数试验编排。

严格顺序是 SET_PARAMS 匹配 ACK→ARM 匹配 ACK→START 匹配 ACK→TRIAL_SUMMARY。
本模块不用实时遥测评分，失败候选不会替换 last_safe。
"""

from dataclasses import dataclass
from collections.abc import Callable
from typing import Protocol

from .models import ControlParams, TrialSummary
from .scoring import ScoreConfig, ScoreResult, score_summary


class TrialSession(Protocol):
    def set_params(self, params: ControlParams) -> int: ...
    def arm(self, param_version: int) -> None: ...
    def start_trial(
        self, mode: int = 0, left_command: int = 0, right_command: int = 0
    ) -> None: ...
    def wait_trial_summary(self, timeout: float = 6.5) -> TrialSummary: ...
    def abort_trial(self) -> None: ...


@dataclass(frozen=True, slots=True)
class TrialResult:
    params: ControlParams
    summary: TrialSummary
    score: ScoreResult


class UnsafeTrialError(RuntimeError):
    """自动阶段遇到本地安全故障，已暂停并尝试恢复 last-safe RAM 参数。"""


class TrialRunner:
    """执行候选并保存最后一组成功且分数有效的参数。"""

    def __init__(
        self,
        session: TrialSession,
        score_config: ScoreConfig,
        on_result: Callable[[TrialResult], None] | None = None,
    ) -> None:
        self.session = session
        self.score_config = score_config
        self.on_result = on_result
        self.last_safe: ControlParams | None = None

    def run_candidate(
        self,
        params: ControlParams,
        *,
        mode: int = 0,
        left_command: int = 0,
        right_command: int = 0,
    ) -> TrialResult:
        """执行一次最多 5 秒的 MCU 本地试验并评分。"""

        try:
            version = self.session.set_params(params)
            self.session.arm(version)
            if (mode == 0 and left_command == 0 and right_command == 0):
                self.session.start_trial()
            else:
                self.session.start_trial(mode, left_command, right_command)
            summary = self.session.wait_trial_summary(timeout=6.5)
        except Exception:
            try:
                self.session.abort_trial()
            except Exception:
                pass
            raise

        if summary.param_version != version:
            raise RuntimeError(
                f"汇总参数版本 {summary.param_version} 与 ACK {version} 不匹配"
            )
        score = score_summary(summary, self.score_config)
        result = TrialResult(params=params, summary=summary, score=score)
        if self.on_result is not None:
            self.on_result(result)
        if not score.failed:
            self.last_safe = params
        elif self.last_safe is not None:
            self.session.set_params(self.last_safe)
        return result
# ----- AI
