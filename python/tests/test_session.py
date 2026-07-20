# ----- AI
"""会话测试 HELLO Session ID、100 ms 心跳生命周期和 SET_PARAMS ACK 版本。"""

import time
import unittest

from autotune.models import ControlParams
from autotune.protocol import Frame, MessageType
from autotune.session import AutotuneSession


class FakeLink:
    def __init__(self):
        self.calls = []
        self.closed = False

    def request(self, message_type, payload, **kwargs):
        self.calls.append((message_type, bytes(payload)))
        if message_type == MessageType.HELLO:
            hello = bytes.fromhex(
                "01 00 02 00 78 56 34 12 01 01 00 00 3F 00 00 00"
            )
            return Frame(MessageType.ACK, 0, 1, hello)
        ack = bytes((int(message_type), 0, 3, 0)) + bytes.fromhex("78 56 34 12")
        return Frame(MessageType.ACK, 0, 2, ack)

    def wait_for_type(self, message_type, timeout):
        return Frame(MessageType.TRIAL_SUMMARY, 0, 0, bytes(118))

    def close(self):
        self.closed = True


class SessionTests(unittest.TestCase):
    def test_set_params_uses_hello_session_and_returns_ack_version(self) -> None:
        link = FakeLink()
        session = AutotuneSession(link, heartbeat_interval=1.0)
        try:
            hello = session.hello(start_heartbeat=False)
            version = session.set_params(ControlParams.safe_defaults())
            self.assertEqual(hello.session_id, 0x12345678)
            self.assertEqual(version, 3)
            self.assertEqual(
                link.calls[-1][1][:4], bytes.fromhex("78 56 34 12")
            )
        finally:
            session.close()

    def test_heartbeat_starts_after_hello_and_stops_on_close(self) -> None:
        link = FakeLink()
        session = AutotuneSession(link, heartbeat_interval=0.02)
        session.hello(start_heartbeat=True)
        time.sleep(0.065)
        session.close()
        count_after_close = sum(
            call[0] == MessageType.HEARTBEAT for call in link.calls
        )
        time.sleep(0.04)
        self.assertGreaterEqual(count_after_close, 2)
        self.assertEqual(
            count_after_close,
            sum(call[0] == MessageType.HEARTBEAT for call in link.calls),
        )
        self.assertTrue(link.closed)


if __name__ == "__main__":
    unittest.main()
# ----- AI
