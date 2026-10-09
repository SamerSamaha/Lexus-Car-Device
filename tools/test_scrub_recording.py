#!/usr/bin/env python3
"""Tests for scrub_recording.py (REQ-015)."""

import contextlib
import io
import tempfile
import unittest
from pathlib import Path

import scrub_recording

# The hypothetical VIN allowlisted in tools/private_data_allowlist.txt; not a real vehicle.
EXAMPLE_VIN = "1M8GDM9AXKP042788"
VIN_HEX_PAIRS = [f"{ord(character):02X}" for character in EXAMPLE_VIN]


def recording_of(adapter_texts, chunk_size=7):
    """A recording whose READ events carry the given adapter texts, cut into small chunks."""
    lines = ["# lexus-head-unit recording 1 source=elm327", "1000 OPEN_OK", "1001 WRITE 303930320d"]
    time = 1002
    for text in adapter_texts:
        data = text.encode("latin-1")
        for start in range(0, len(data), chunk_size):
            lines.append(f"{time} READ {data[start:start + chunk_size].hex()}")
            time += 1
    lines.append(f"{time} CLOSE")
    return "\n".join(lines) + "\n"


def read_stream(recording):
    payloads = [line.split()[2] for line in recording.splitlines() if " READ " in line and len(line.split()) > 2]
    return bytes.fromhex("".join(payloads)).decode("latin-1")


class ScrubRecordingTest(unittest.TestCase):
    def assert_scrubbed(self, adapter_texts):
        original = recording_of(adapter_texts)
        scrubbed, replaced, remaining = scrub_recording.scrub_recording(original)
        self.assertEqual(remaining, 0)
        self.assertGreater(replaced, 0)
        stream = read_stream(scrubbed)
        self.assertNotIn(EXAMPLE_VIN, stream)
        self.assertEqual(len(stream), len(read_stream(original)))
        self.assertEqual(len(scrubbed.splitlines()), len(original.splitlines()))
        self.assertEqual(scrub_recording.remaining_vins(stream), 0)
        return stream

    def test_mode_09_reply_with_spaces_over_three_frames(self):
        pairs = ["49", "02", "01"] + VIN_HEX_PAIRS
        reply = "014\r0: " + " ".join(pairs[0:6]) + "\r1: " + " ".join(pairs[6:13]) + "\r2: " + " ".join(pairs[13:20]) + "\r\r>"
        stream = self.assert_scrubbed(["SEARCHING...\r", reply])
        self.assertIn("0: 49 02 01 30 30 30", stream)

    def test_mode_09_reply_without_spaces(self):
        pairs = ["49", "02", "01"] + VIN_HEX_PAIRS
        reply = "014\r0:" + "".join(pairs[0:6]) + "\r1:" + "".join(pairs[6:13]) + "\r2:" + "".join(pairs[13:20]) + "\r\r>"
        self.assert_scrubbed([reply])

    def test_single_line_reply_and_plain_text(self):
        self.assert_scrubbed(["4902" + "01" + "".join(VIN_HEX_PAIRS) + "\r>", "VIN " + EXAMPLE_VIN + "\r>"])

    def test_recording_without_a_vin_is_unchanged(self):
        original = recording_of(["41 0D 3C\r\r>", "ELM327 v1.5\r\r>"])
        scrubbed, replaced, remaining = scrub_recording.scrub_recording(original)
        self.assertEqual((scrubbed, replaced, remaining), (original, 0, 0))

    def test_command_line_writes_only_a_clean_output(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "drive.rec"
            target = Path(folder) / "drive.scrubbed.rec"
            source.write_text(recording_of(["VIN " + EXAMPLE_VIN + "\r>"]), encoding="ascii")
            with contextlib.redirect_stdout(io.StringIO()):
                exit_code = scrub_recording.main([str(source), "--output", str(target)])
            self.assertEqual(exit_code, scrub_recording.EXIT_CODE_WRITTEN)
            self.assertNotIn(EXAMPLE_VIN, read_stream(target.read_text(encoding="ascii")))
            with contextlib.redirect_stderr(io.StringIO()):
                missing = scrub_recording.main([str(Path(folder) / "none.rec"), "--output", str(target)])
            self.assertEqual(missing, scrub_recording.EXIT_CODE_COULD_NOT_RUN)


if __name__ == "__main__":
    unittest.main()
