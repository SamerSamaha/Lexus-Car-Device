#!/usr/bin/env python3
"""Tests for measure/summarise_session_log.py."""

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "measure"))

import summarise_session_log as summariser  # noqa: E402

HEADER = "milliseconds,event,state,trigger,from,to,requests_sent,samples_emitted,malformed_inputs"


def write_log(folder, rows):
    path = Path(folder) / "session.csv"
    path.write_text("\n".join([HEADER] + rows) + "\n", encoding="ascii")
    return path


class SummariseSessionLogTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)

    def log_with_rates(self, before, during, after, lost_at=None):
        """Counter rows every 10 s: 0-60 s at `before`, 60-120 s at `during`, 120-180 s at `after` requests/s."""
        rows = ["0,transition,Connected,HandshakeSucceeded,Connecting,Connected,,,"]
        requests = 0
        for second in range(0, 181, 10):
            rows.append(f"{second * 1000},counters,Connected,,,,{requests},0,0")
            rate = before if second < 60 else during if second < 120 else after
            requests += rate * 10
        if lost_at is not None:
            rows.append(f"{lost_at * 1000},transition,Error,LinkLost,Connected,Error,,,")
        return write_log(self.temporary_directory.name, rows)

    def test_rates_with_and_without_audio_and_the_20_percent_rule(self):
        counters, transitions = summariser.read_log(self.log_with_rates(20, 18, 20))
        summary = summariser.summarise(counters, transitions, 60.0, 120.0)
        self.assertIn("request rate without audio: 20.00/s", summary)
        self.assertIn("request rate with audio: 18.00/s", summary)
        self.assertIn("change: -10.0 % (within 20 %)", summary)
        self.assertIn("transitions out of Connected: 0", summary)

    def test_a_drop_beyond_20_percent_and_a_lost_link_during_audio_are_reported(self):
        counters, transitions = summariser.read_log(self.log_with_rates(20, 10, 20, lost_at=90))
        summary = summariser.summarise(counters, transitions, 60.0, 120.0)
        self.assertIn("change: -50.0 % (outside 20 %)", summary)
        self.assertIn("transitions out of Connected: 1", summary)
        self.assertIn("90.0 s LinkLost -> Error (during audio)", summary)

    def test_too_few_rows_give_unknown_rates(self):
        path = write_log(self.temporary_directory.name, ["0,counters,Connected,,,,0,0,0"])
        counters, transitions = summariser.read_log(path)
        summary = summariser.summarise(counters, transitions, 0.0, 10.0)
        self.assertIn("request rate without audio: unknown", summary)
        self.assertNotIn("change:", summary)

    def test_command_line(self):
        path = self.log_with_rates(20, 20, 20)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            exit_code = summariser.main([str(path), "--audio-start-seconds", "60", "--audio-end-seconds", "120"])
        self.assertEqual(exit_code, summariser.EXIT_CODE_OK)
        self.assertIn("change: +0.0 %", output.getvalue())
        with contextlib.redirect_stderr(io.StringIO()):
            missing = summariser.main(["/nonexistent.csv", "--audio-start-seconds", "0", "--audio-end-seconds", "1"])
        self.assertEqual(missing, summariser.EXIT_CODE_COULD_NOT_RUN)


if __name__ == "__main__":
    unittest.main()
