# ----- AI
"""1-3 二进制协议的可重复测试向量。"""

import unittest

from autotune.protocol import (
    FLAG_ACK_REQUIRED,
    MAX_PAYLOAD_LENGTH,
    Frame,
    FrameDecoder,
    MessageType,
    crc16_ccitt_false,
    decode_hello_ack,
    encode_frame,
)


class Crc16Tests(unittest.TestCase):
    def test_standard_check_value(self) -> None:
        self.assertEqual(crc16_ccitt_false(b"123456789"), 0x29B1)


class FrameCodecTests(unittest.TestCase):
    def test_encoder_matches_shared_golden_vector(self) -> None:
        frame = Frame(
            message_type=MessageType.HELLO,
            flags=FLAG_ACK_REQUIRED,
            sequence=0x1234,
            payload=b"abc",
        )
        expected = bytes.fromhex(
            "A5 5A 01 01 01 34 12 03 00 61 62 63 07 58"
        )
        self.assertEqual(encode_frame(frame), expected)

    def test_decoder_accepts_fragmented_frame(self) -> None:
        encoded = encode_frame(Frame(MessageType.GET_STATUS, 0, 7, b""))
        decoder = FrameDecoder()

        self.assertEqual(decoder.feed(encoded[:4]), [])
        self.assertEqual(
            decoder.feed(encoded[4:]),
            [Frame(MessageType.GET_STATUS, 0, 7, b"")],
        )

    def test_decoder_extracts_stuck_frames_after_noise(self) -> None:
        first = Frame(MessageType.HEARTBEAT, 0, 8, b"\x44\x33\x22\x11")
        second = Frame(MessageType.GET_STATUS, 0, 9, b"")
        decoder = FrameDecoder()

        decoded = decoder.feed(b"\x00\xA5\x00noise" + encode_frame(first) + encode_frame(second))

        self.assertEqual(decoded, [first, second])
        self.assertGreaterEqual(decoder.noise_bytes, 1)

    def test_crc_error_never_emits_a_command_and_resynchronizes(self) -> None:
        damaged = bytearray(encode_frame(Frame(MessageType.ARM, 0, 10, b"\x01")))
        damaged[-1] ^= 0x80
        good = Frame(MessageType.GET_STATUS, 0, 11, b"")
        decoder = FrameDecoder()

        decoded = decoder.feed(bytes(damaged) + encode_frame(good))

        self.assertEqual(decoded, [good])
        self.assertEqual(decoder.crc_errors, 1)

    def test_oversized_length_is_rejected_without_waiting_for_payload(self) -> None:
        invalid_header = bytes.fromhex("A5 5A 01 01 00 01 00 81 00")
        good = Frame(MessageType.GET_STATUS, 0, 12, b"")
        decoder = FrameDecoder()

        decoded = decoder.feed(invalid_header + encode_frame(good))

        self.assertEqual(decoded, [good])
        self.assertEqual(decoder.length_errors, 1)

    def test_encoder_rejects_payload_above_protocol_limit(self) -> None:
        frame = Frame(
            MessageType.SET_PARAMS,
            FLAG_ACK_REQUIRED,
            13,
            bytes(MAX_PAYLOAD_LENGTH + 1),
        )
        with self.assertRaises(ValueError):
            encode_frame(frame)

    def test_hello_ack_has_stable_cross_language_layout(self) -> None:
        payload = bytes.fromhex(
            "01 00 34 12 78 56 34 12 01 01 00 00 3F 00 00 00"
        )

        hello = decode_hello_ack(payload)

        self.assertEqual(hello.acknowledged_type, MessageType.HELLO)
        self.assertEqual(hello.status, 0)
        self.assertEqual(hello.param_version, 0x1234)
        self.assertEqual(hello.session_id, 0x12345678)
        self.assertEqual(hello.protocol_version, 1)
        self.assertEqual(hello.firmware_version, (1, 0, 0))
        self.assertEqual(hello.capabilities, 0x3F)


if __name__ == "__main__":
    unittest.main()
# ----- AI
