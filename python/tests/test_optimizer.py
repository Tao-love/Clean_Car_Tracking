# ----- AI
"""有边界坐标搜索不越界，不把失败候选当最优，且始终保留 last-safe。"""

import unittest

from autotune.models import ControlParams
from autotune.optimizer import BoundedCoordinateSearch, Evaluation


class OptimizerTests(unittest.TestCase):
    def test_search_respects_bounds_and_keeps_last_safe(self) -> None:
        initial = ControlParams.safe_defaults()
        seen: list[int] = []

        def evaluate(candidate: ControlParams) -> Evaluation:
            seen.append(candidate.base_speed)
            if candidate.base_speed >= 30:
                return Evaluation(score=1_000_000.0, failed=True)
            return Evaluation(score=abs(20 - candidate.base_speed), failed=False)

        search = BoundedCoordinateSearch(
            fields={"base_speed": (0, 40, 10)}, max_rounds=4
        )
        result = search.run(initial, evaluate)

        self.assertTrue(all(0 <= value <= 40 for value in seen))
        self.assertEqual(result.best.base_speed, 20)
        self.assertEqual(result.last_safe.base_speed, 20)
        self.assertFalse(result.best_evaluation.failed)


if __name__ == "__main__":
    unittest.main()
# ----- AI
