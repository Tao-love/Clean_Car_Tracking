# ----- AI
"""VOFA+ 转发失败只计丢弃，不能中断试验。"""

import unittest

from autotune.messages import Telemetry
from autotune.vofa import VofaForwarder


class FailingSocket:
    def sendto(self, data, address):
        raise OSError("VOFA not running")

    def close(self):
        pass


class VofaTests(unittest.TestCase):
    def test_send_failure_is_contained(self) -> None:
        forwarder = VofaForwarder(enabled=True, socket_factory=lambda: FailingSocket())
        telemetry = Telemetry(1, 3, 3, 0, True, 100, 10, 11, 9, 8, 200, 201, 2)

        forwarder.forward(telemetry)

        self.assertEqual(forwarder.dropped_packets, 1)


if __name__ == "__main__":
    unittest.main()
# ----- AI
