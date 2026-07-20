# ----- AI
"""试验编排必须严格 SET_PARAMS→ARM→START→SUMMARY，失败不覆盖回滚点。"""

import unittest

from autotune.models import ControlParams
from autotune.scoring import ScoreConfig
from autotune.trial import TrialRunner
from test_scoring import make_summary


class FakeSession:
    def __init__(self, summary):
        self.calls: list[str] = []
        self.summary = summary

    def set_params(self, params: ControlParams) -> int:
        self.calls.append("set_params")
        return 7

    def arm(self, param_version: int) -> None:
        self.calls.append(f"arm:{param_version}")

    def start_trial(self) -> None:
        self.calls.append("start")

    def wait_trial_summary(self, timeout: float = 6.5):
        self.calls.append("summary")
        return self.summary

    def abort_trial(self) -> None:
        self.calls.append("abort")


class TrialRunnerTests(unittest.TestCase):
    def test_safe_trial_updates_last_safe_after_required_order(self) -> None:
        session = FakeSession(make_summary(param_version=7))
        params = ControlParams.safe_defaults()
        runner = TrialRunner(session, ScoreConfig.defaults())

        result = runner.run_candidate(params)

        self.assertEqual(session.calls, ["set_params", "arm:7", "start", "summary"])
        self.assertFalse(result.score.failed)
        self.assertEqual(runner.last_safe, params)

    def test_faulted_trial_does_not_replace_last_safe(self) -> None:
        safe_params = ControlParams.safe_defaults()
        failed_params = ControlParams.safe_defaults()
        session = FakeSession(make_summary(param_version=7, fault=1, stop_reason=4))
        runner = TrialRunner(session, ScoreConfig.defaults())
        runner.last_safe = safe_params

        result = runner.run_candidate(failed_params)

        self.assertTrue(result.score.failed)
        self.assertIs(runner.last_safe, safe_params)
        self.assertEqual(session.calls[-1], "set_params")


if __name__ == "__main__":
    unittest.main()
# ----- AI
