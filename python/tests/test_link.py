# ----- AI
"""串口链路测试 ACK Sequence 匹配、有限重试和异步帧等待。"""

from collections import deque
import threading
import time
import unittest

from autotune.link import SerialLink
from autotune.protocol import Frame, FrameDecoder, MessageType, encode_frame


class FakeTransport:
    def __init__(self, responder):
        self._rx = deque()
        self._lock = threading.Lock()
        self.responder = responder
        self.write_count = 0
        self.closed = False

    def write(self, data: bytes) -> int:
        self.write_count += 1
        response = self.responder(bytes(data), self.write_count)
        if response:
            with self._lock:
                self._rx.extend(response)
        return len(data)

    def read(self, size: int) -> bytes:
        deadline = time.monotonic() + 0.01
        while time.monotonic() < deadline:
            with self._lock:
                if self._rx:
                    result = bytearray()
                    while self._rx and len(result) < size:
                        result.append(self._rx.popleft())
                    return bytes(result)
            time.sleep(0.001)
        return b""

    def close(self) -> None:
        self.closed = True


def ack_for_request(data: bytes, status: int = 0) -> bytes:
    request = FrameDecoder().feed(data)[0]
    payload = bytes((int(request.message_type), status)) + b"\x07\x00" + b"\x78\x56\x34\x12"
    return encode_frame(Frame(MessageType.ACK, 0, request.sequence, payload))


class SerialLinkTests(unittest.TestCase):
    def test_request_retries_same_sequence_then_accepts_matching_ack(self) -> None:
        transport = FakeTransport(
            lambda data, count: None if count == 1 else ack_for_request(data)
        )
        link = SerialLink(transport)
        try:
            response = link.request(
                MessageType.GET_STATUS, b"\x78\x56\x34\x12",
                timeout=0.03, retries=1,
            )
            self.assertEqual(response.message_type, MessageType.ACK)
            self.assertEqual(transport.write_count, 2)
        finally:
            link.close()

    def test_wait_for_type_receives_unsolicited_summary(self) -> None:
        transport = FakeTransport(lambda data, count: None)
        link = SerialLink(transport)
        try:
            summary = Frame(MessageType.TRIAL_SUMMARY, 0, 0, bytes(118))
            with transport._lock:
                transport._rx.extend(encode_frame(summary))
            received = link.wait_for_type(MessageType.TRIAL_SUMMARY, timeout=0.2)
            self.assertEqual(received, summary)
        finally:
            link.close()

    def test_close_closes_owned_transport(self) -> None:
        transport = FakeTransport(lambda data, count: None)
        link = SerialLink(transport)
        link.close()
        self.assertTrue(transport.closed)


if __name__ == "__main__":
    unittest.main()
# ----- AI
