# ----- AI
"""MCU Flash 双槽 80 字节记录的离线解码与回退测试模型。"""

from dataclasses import dataclass
import struct

from .models import ControlParams
from .protocol import crc16_ccitt_false

FLASH_RECORD_MAGIC = 0x31504941  # 小端字节为 "AIP1"
FLASH_RECORD_SCHEMA = 1
FLASH_RECORD_SIZE = 80


@dataclass(frozen=True, slots=True)
class FlashRecord:
    generation: int
    param_version: int
    params: ControlParams

    def to_wire(self) -> bytes:
        prefix = struct.pack(
            "<IHHIHH",
            FLASH_RECORD_MAGIC,
            FLASH_RECORD_SCHEMA,
            58,
            self.generation,
            self.param_version,
            0,
        ) + self.params.to_wire()
        crc = crc16_ccitt_false(prefix)
        return prefix + struct.pack("<H", crc) + b"\xFF" * 4

    @classmethod
    def from_wire(cls, data: bytes) -> "FlashRecord | None":
        if len(data) < FLASH_RECORD_SIZE:
            return None
        data = data[:FLASH_RECORD_SIZE]
        magic, schema, length, generation, version, _ = struct.unpack(
            "<IHHIHH", data[:16]
        )
        if (
            magic != FLASH_RECORD_MAGIC
            or schema != FLASH_RECORD_SCHEMA
            or length != 58
            or crc16_ccitt_false(data[:74]) != int.from_bytes(data[74:76], "little")
        ):
            return None
        try:
            params = ControlParams.from_wire(data[16:74])
        except ValueError:
            return None
        return cls(generation, version, params)


def choose_latest(slot_a: bytes, slot_b: bytes) -> FlashRecord | None:
    """选择 CRC 正确且 generation 最大的槽；两槽坏时返回 None。"""

    records = [
        item for item in (FlashRecord.from_wire(slot_a), FlashRecord.from_wire(slot_b))
        if item is not None
    ]
    return max(records, key=lambda item: item.generation, default=None)
# ----- AI
