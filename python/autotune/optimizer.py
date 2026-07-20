# ----- AI
"""
有边界、可解释的坐标搜索。

优化器只生成字段边界内的候选，不负责启动电机。
调用者的 evaluator 必须完成一次独立安全试验并返回评分。
"""

from dataclasses import dataclass, replace
from typing import Callable

from .models import ControlParams


@dataclass(frozen=True, slots=True)
class Evaluation:
    score: float
    failed: bool


@dataclass(frozen=True, slots=True)
class SearchResult:
    best: ControlParams
    best_evaluation: Evaluation
    last_safe: ControlParams
    evaluations: int


class BoundedCoordinateSearch:
    """逐字段尝试正/负固定步长的确定坐标搜索。"""

    def __init__(
        self,
        fields: dict[str, tuple[int, int, int]],
        max_rounds: int = 3,
    ) -> None:
        if max_rounds <= 0:
            raise ValueError("max_rounds 必须大于 0")
        for name, (lower, upper, step) in fields.items():
            if lower > upper or step <= 0 or not hasattr(ControlParams.safe_defaults(), name):
                raise ValueError(f"无效搜索边界: {name}")
        self.fields = dict(fields)
        self.max_rounds = max_rounds

    def run(
        self,
        initial: ControlParams,
        evaluator: Callable[[ControlParams], Evaluation],
    ) -> SearchResult:
        """评估初值后逐轮搜索，失败候选永不替换 best/last-safe。"""

        best = initial
        best_eval = evaluator(initial)
        evaluations = 1
        if best_eval.failed:
            raise RuntimeError("初始参数未通过安全试验，不能作为搜索起点")
        last_safe = initial
        visited = {initial}

        for _ in range(self.max_rounds):
            improved = False
            for name, (lower, upper, step) in self.fields.items():
                center = getattr(best, name)
                for raw_value in (center - step, center + step):
                    value = min(max(raw_value, lower), upper)
                    candidate = replace(best, **{name: value})
                    if candidate in visited:
                        continue
                    visited.add(candidate)
                    evaluation = evaluator(candidate)
                    evaluations += 1
                    if not evaluation.failed and evaluation.score < best_eval.score:
                        best = candidate
                        best_eval = evaluation
                        last_safe = candidate
                        improved = True
            if not improved:
                break
        return SearchResult(best, best_eval, last_safe, evaluations)
# ----- AI
