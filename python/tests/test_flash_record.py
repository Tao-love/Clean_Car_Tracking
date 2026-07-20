# ----- AI
"""Flash 双槽记录的 magic/schema/generation/CRC 选择契约。"""

import unittest

from autotune.flash_record import FlashRecord, choose_latest
from autotune.models import ControlParams


class FlashRecordTests(unittest.TestCase):
    def test_newer_valid_generation_wins(self) -> None:
        params = ControlParams.safe_defaults()
        old = FlashRecord(3, 7, params).to_wire()
        new = FlashRecord(4, 8, params).to_wire()
        selected = choose_latest(old, new)
        self.assertEqual(selected.generation, 4)
        self.assertEqual(selected.param_version, 8)

    def test_corrupt_new_slot_falls_back_to_old(self) -> None:
        params = ControlParams.safe_defaults()
        old = FlashRecord(3, 7, params).to_wire()
        new = bytearray(FlashRecord(4, 8, params).to_wire())
        new[20] ^= 0x80
        self.assertEqual(choose_latest(old, bytes(new)).generation, 3)

    def test_two_invalid_slots_return_none(self) -> None:
        self.assertIsNone(choose_latest(bytes(80), bytes(80)))


if __name__ == "__main__":
    unittest.main()
# ----- AI
