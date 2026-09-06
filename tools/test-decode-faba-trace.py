import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("trace_decoder", Path(__file__).with_name("decode-faba-trace.py"))
decoder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(decoder)
FIXTURE = Path(__file__).resolve().parents[1] / "diagnostics/pixl-faba-build/synthetic.trace"


class DecodeTest(unittest.TestCase):
    def test_real_c_export_and_clock_wrap(self):
        capture = decoder.decode(FIXTURE.read_bytes())
        self.assertEqual(capture["uid"], "04:e7:dd:17:bc:2a:81")
        self.assertEqual(capture["slot"], 2)
        events = capture["events"]
        self.assertEqual(events[1]["command"], "REQA")
        self.assertEqual(events[2]["command"], "READ")
        self.assertEqual(events[2]["state"], "ACTIVE")
        self.assertTrue(events[3]["truncated"])
        self.assertEqual(len(events[3]["data"].split()), 12)
        self.assertLess(events[-1]["rtc_elapsed_ms"], 1)
        self.assertEqual(events[1]["cpu_delta_us_modulo"], 5)

    def test_parity_packed_phone_frames(self):
        for bits, raw, expected in [
            (18, "934100", "9320"),
            (81, "93e1202670bebbad afc100", "93708804e7ddb65fc1"),
            (36, "3011282109", "30084a24"),
            (36, "50015e6906", "500057cd"),
        ]:
            data, parity = decoder.unwrap_rx(bits, bytes.fromhex(raw))
            self.assertEqual(data.hex(), expected)
            self.assertTrue(parity)
        self.assertEqual(decoder.command(72, bytes.fromhex("93708804e7ddb65fc1")), "SELECT CL1")

    def test_hardware_timing_wrap_and_config(self):
        raw = bytearray(FIXTURE.read_bytes())
        raw[46] = 1
        data = decoder.struct.pack("<III", 0xfffffff0, 0x550, 0x590)
        decoder.EVENT.pack_into(raw, decoder.HEADER.size, 0, 0, 96, 11, 1, data)
        event = decoder.decode(raw)["events"][0]
        self.assertEqual(event["rx_end_to_tx_start_us"], 86.0)
        self.assertEqual(event["tx_start_irq_delay_us"], 4.0)
        data = decoder.struct.pack("<III", 1232, 4096, 3)
        decoder.EVENT.pack_into(raw, decoder.HEADER.size, 0, 0, 96, 12, 1, data)
        event = decoder.decode(raw)["events"][0]
        self.assertEqual(event["frame_delay_mode"], "WindowGrid")
        self.assertEqual(event["frame_delay_min_carrier_cycles"], 1232)
        data = decoder.struct.pack("<III", 0xfffffff0, 0x550, 0x1110)
        decoder.EVENT.pack_into(raw, decoder.HEADER.size, 0, 0, 96, 7, 1, data)
        self.assertEqual(decoder.decode(raw)["events"][0]["tx_duration_us"], 188.0)

    def test_tail_retention_marks_missing_middle(self):
        header = list(decoder.HEADER.unpack_from(FIXTURE.read_bytes()))
        header[3], header[4], header[14] = 128, 42, bytes([1, 1])
        raw = decoder.HEADER.pack(*header) + b"".join(
            decoder.EVENT.pack(i, i, 0, 1, 0, bytes(12)) for i in range(128))
        result = decoder.decode(raw)
        self.assertEqual(result["retention"], "first32+last96")
        self.assertEqual(result["omitted_middle_events"], 42)

    def test_receive_error_status(self):
        raw = bytearray(FIXTURE.read_bytes())
        decoder.EVENT.pack_into(raw, decoder.HEADER.size, 0, 0, 32, 10, 0,
                                (5).to_bytes(4, "little") + bytes(8))
        event = decoder.decode(raw)["events"][0]
        self.assertEqual(event["kind"], "RX_ERROR")
        self.assertEqual(event["rx_status"], 5)

    def test_partial_or_trailing_data_rejected(self):
        raw = FIXTURE.read_bytes()
        for damaged in (raw[:20], raw[:-1], raw + b"\0"):
            with self.assertRaises(ValueError):
                decoder.decode(damaged)

    def test_unknown_format_or_event_rejected(self):
        for offset in (0, 8, 10, decoder.HEADER.size + 10):
            raw = bytearray(FIXTURE.read_bytes())
            raw[offset] = 255
            with self.assertRaises(ValueError):
                decoder.decode(raw)


if __name__ == "__main__":
    unittest.main()
