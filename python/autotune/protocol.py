# ----- AI
"""
二进制链路协议。

职责：实现与 MSPM0 固件逐字节一致的 CRC、帧编码和流式解码。
边界：本模块不打开串口、不重试命令、不改变小车状态。
字节序：所有多字节整数均为小端。
"""

from dataclasses import dataclass
from enum import IntEnum

MAGIC = b"\xA5\x5A"
PROTOCOL_VERSION = 1
MAX_PAYLOAD_LENGTH = 128
FLAG_ACK_REQUIRED = 0x01
FLAG_ERROR = 0x02
FLAG_HIGH_PRIORITY = 0x04


class MessageType(IntEnum):
    """首版命令与回传消息类型。"""

    HELLO = 0x01
    HEARTBEAT = 0x02
    SET_PARAMS = 0x03
    GET_STATUS = 0x04
    ARM = 0x05
    START_TRIAL = 0x06
    ABORT_TRIAL = 0x07
    CLEAR_FAULT = 0x08
    SET_TELEMETRY = 0x09
    COMMIT_PARAMS = 0x0A
    ACK = 0x80
    NACK = 0x81
    STATUS = 0x82
    TELEMETRY = 0x83
    TRIAL_SUMMARY = 0x84
    EVENT = 0x85


@dataclass(frozen=True, slots=True)
class Frame:
    """已通过长度与 CRC 校验的一帧数据。"""

    message_type: int
    flags: int
    sequence: int
    payload: bytes
    version: int = PROTOCOL_VERSION

    def __post_init__(self) -> None:
        if not 0 <= int(self.message_type) <= 0xFF:
            raise ValueError("message_type 必须是 0..255")
        if not 0 <= self.flags <= 0xFF:
            raise ValueError("flags 必须是 0..255")
        if not 0 <= self.sequence <= 0xFFFF:
            raise ValueError("sequence 必须是 0..65535")
        if not 0 <= self.version <= 0xFF:
            raise ValueError("version 必须是 0..255")


@dataclass(frozen=True, slots=True)
class HelloAck:
    """MCU 对 HELLO 的固定布局应答。"""

    acknowledged_type: int
    status: int
    param_version: int
    session_id: int
    protocol_version: int
    firmware_version: tuple[int, int, int]
    capabilities: int


def crc16_ccitt_false(data: bytes | bytearray | memoryview) -> int:
    """CRC-16/CCITT-FALSE：poly=0x1021, init=0xFFFF, xorout=0。"""

    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def encode_frame(frame: Frame) -> bytes:
    """把一个合法帧编码为 UART 字节流。"""

    payload = bytes(frame.payload)
    if len(payload) > MAX_PAYLOAD_LENGTH:
        raise ValueError(f"payload 不得超过 {MAX_PAYLOAD_LENGTH} 字节")
    body = bytes((frame.version, int(frame.message_type), frame.flags))
    body += frame.sequence.to_bytes(2, "little")
    body += len(payload).to_bytes(2, "little")
    body += payload
    return MAGIC + body + crc16_ccitt_false(body).to_bytes(2, "little")


def decode_hello_ack(payload: bytes) -> HelloAck:
    """解码 16 字节 HELLO ACK；长度不符时立即拒绝。"""

    if len(payload) != 16:
        raise ValueError("HELLO ACK payload 必须为 16 字节")
    return HelloAck(
        acknowledged_type=payload[0],
        status=payload[1],
        param_version=int.from_bytes(payload[2:4], "little"),
        session_id=int.from_bytes(payload[4:8], "little"),
        protocol_version=payload[8],
        firmware_version=(payload[9], payload[10], payload[11]),
        capabilities=int.from_bytes(payload[12:16], "little"),
    )


class FrameDecoder:
    """可反复 feed 任意分片的有界流式解码器。"""

    def __init__(self) -> None:
        self._buffer = bytearray()
        self.noise_bytes = 0
        self.crc_errors = 0
        self.length_errors = 0

    def feed(self, data: bytes | bytearray | memoryview) -> list[Frame]:
        """加入新字节，返回本次能够完整解出的所有帧。"""

        self._buffer.extend(data)
        frames: list[Frame] = []
        while True:
            if not self._seek_magic():
                break
            if len(self._buffer) < 9:
                break

            payload_length = int.from_bytes(self._buffer[7:9], "little")
            if payload_length > MAX_PAYLOAD_LENGTH:
                self.length_errors += 1
                del self._buffer[0]
                continue

            frame_length = 11 + payload_length
            if len(self._buffer) < frame_length:
                break

            body_end = 9 + payload_length
            expected_crc = int.from_bytes(
                self._buffer[body_end : body_end + 2], "little"
            )
            actual_crc = crc16_ccitt_false(self._buffer[2:body_end])
            if actual_crc != expected_crc:
                self.crc_errors += 1
                del self._buffer[0]
                continue

            message_value = self._buffer[3]
            try:
                message_type: int = MessageType(message_value)
            except ValueError:
                message_type = message_value
            frames.append(
                Frame(
                    message_type=message_type,
                    flags=self._buffer[4],
                    sequence=int.from_bytes(self._buffer[5:7], "little"),
                    payload=bytes(self._buffer[9:body_end]),
                    version=self._buffer[2],
                )
            )
            del self._buffer[:frame_length]
        return frames

    def _seek_magic(self) -> bool:
        """丢弃 Magic 之前的噪声，保留末尾单个 0xA5 等待下一片。"""

        index = self._buffer.find(MAGIC)
        if index >= 0:
            if index:
                self.noise_bytes += index
                del self._buffer[:index]
            return True

        keep = 1 if self._buffer.endswith(MAGIC[:1]) else 0
        discarded = len(self._buffer) - keep
        if discarded:
            self.noise_bytes += discarded
            del self._buffer[:discarded]
        return False
# ----- AI
