# ----- AI
"""
JDY-31 Windows COM 口的唯一所有者。

读线程只做字节读取、帧解码和入队；命令请求串行化，
且每次重试使用相同 Sequence，使 MCU 去重能返回原 ACK 而不重复执行。
"""

from collections import deque
from collections.abc import Callable
import threading
import time
from typing import Protocol

from .protocol import Frame, FrameDecoder, MessageType, encode_frame


class ByteTransport(Protocol):
    def read(self, size: int) -> bytes: ...
    def write(self, data: bytes) -> int: ...
    def close(self) -> None: ...


class LinkTimeout(TimeoutError):
    """有限重试后仍未收到匹配 Sequence 响应。"""


class ProtocolNack(RuntimeError):
    """MCU 明确拒绝命令。"""

    def __init__(self, frame: Frame) -> None:
        self.frame = frame
        self.status = frame.payload[1] if len(frame.payload) >= 2 else -1
        super().__init__(f"MCU NACK status={self.status} seq={frame.sequence}")


RawFrameCallback = Callable[[str, bytes], None]
FrameCallback = Callable[[Frame], None]


class SerialLink:
    """具有后台解码、Sequence 匹配和有限重试的串口链路。"""

    def __init__(
        self,
        transport: ByteTransport,
        raw_callback: RawFrameCallback | None = None,
        frame_callback: FrameCallback | None = None,
    ) -> None:
        self._transport = transport
        self._raw_callback = raw_callback
        self._frame_callback = frame_callback
        self._decoder = FrameDecoder()
        self._frames: deque[Frame] = deque()
        self._condition = threading.Condition()
        self._request_lock = threading.Lock()
        self._sequence = 0
        self._closed = False
        self._reader_error: BaseException | None = None
        self._reader = threading.Thread(
            target=self._reader_loop, name="autotune-serial-reader", daemon=True
        )
        self._reader.start()

    @classmethod
    def open(
        cls,
        port: str,
        baudrate: int = 9600,
        raw_callback: RawFrameCallback | None = None,
        frame_callback: FrameCallback | None = None,
    ) -> "SerialLink":
        """以 8N1、50 ms 读超时打开 JDY-31 对应 COM 口。"""

        import serial

        transport = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.05,
            write_timeout=0.25,
        )
        return cls(transport, raw_callback, frame_callback)

    @staticmethod
    def list_ports() -> list[tuple[str, str]]:
        """列出 Windows 当前可见 COM 口和描述，不自动打开。"""

        from serial.tools import list_ports

        return [(item.device, item.description) for item in list_ports.comports()]

    def request(
        self,
        message_type: int,
        payload: bytes,
        *,
        expected_types: tuple[int, ...] = (MessageType.ACK, MessageType.NACK),
        timeout: float = 0.25,
        retries: int = 2,
    ) -> Frame:
        """发送一个命令并等待匹配 Sequence；最多发送 retries+1 次。"""

        if retries < 0 or timeout <= 0:
            raise ValueError("retries 不得为负，timeout 必须大于 0")
        with self._request_lock:
            sequence = self._next_sequence()
            frame = Frame(message_type, 1, sequence, bytes(payload))
            encoded = encode_frame(frame)
            for _ in range(retries + 1):
                self._raise_if_unusable()
                if self._raw_callback is not None:
                    self._raw_callback("tx", encoded)
                written = self._transport.write(encoded)
                if written != len(encoded):
                    raise OSError(f"串口只写入 {written}/{len(encoded)} 字节")
                response = self._wait_matching(
                    lambda item: item.sequence == sequence
                    and int(item.message_type) in tuple(map(int, expected_types)),
                    timeout,
                )
                if response is not None:
                    if int(response.message_type) == int(MessageType.NACK):
                        raise ProtocolNack(response)
                    return response
            raise LinkTimeout(
                f"命令 type=0x{int(message_type):02X} seq={sequence} 超时"
            )

    def wait_for_type(self, message_type: int, timeout: float) -> Frame:
        """等待无请求 Sequence 的异步帧，如 TRIAL_SUMMARY。"""

        response = self._wait_matching(
            lambda item: int(item.message_type) == int(message_type), timeout
        )
        if response is None:
            raise LinkTimeout(f"等待 type=0x{int(message_type):02X} 超时")
        return response

    def close(self) -> None:
        """停止读线程并释放 COM 口；可重复调用。"""

        with self._condition:
            if self._closed:
                return
            self._closed = True
            self._condition.notify_all()
        self._transport.close()
        self._reader.join(timeout=0.5)

    def _reader_loop(self) -> None:
        try:
            while True:
                with self._condition:
                    if self._closed:
                        return
                chunk = self._transport.read(256)
                if not chunk:
                    continue
                if self._raw_callback is not None:
                    self._raw_callback("rx", bytes(chunk))
                frames = self._decoder.feed(chunk)
                if not frames:
                    continue
                with self._condition:
                    # 遥测只交给日志/VOFA 回调，不进入命令等待队列，避免长时整定无界增长。
                    self._frames.extend(
                        frame for frame in frames
                        if int(frame.message_type) != int(MessageType.TELEMETRY)
                    )
                    self._condition.notify_all()
                if self._frame_callback is not None:
                    for frame in frames:
                        self._frame_callback(frame)
        except BaseException as error:
            with self._condition:
                if not self._closed:
                    self._reader_error = error
                self._condition.notify_all()

    def _wait_matching(
        self, predicate: Callable[[Frame], bool], timeout: float
    ) -> Frame | None:
        deadline = time.monotonic() + timeout
        with self._condition:
            while True:
                self._raise_if_unusable()
                for index, frame in enumerate(self._frames):
                    if predicate(frame):
                        del self._frames[index]
                        return frame
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                self._condition.wait(remaining)

    def _raise_if_unusable(self) -> None:
        if self._reader_error is not None:
            raise OSError("串口读线程失败") from self._reader_error
        if self._closed:
            raise OSError("串口链路已关闭")

    def _next_sequence(self) -> int:
        self._sequence = (self._sequence + 1) & 0xFFFF
        if self._sequence == 0:
            self._sequence = 1
        return self._sequence
# ----- AI
