#!/usr/bin/env python3
"""Tests for check_can_read_only.py."""

import contextlib
import io
import tempfile
import unittest
from pathlib import Path

import check_can_read_only


def run_main(repository_root):
    standard_output = io.StringIO()
    standard_error = io.StringIO()
    with contextlib.redirect_stdout(standard_output), contextlib.redirect_stderr(standard_error):
        exit_code = check_can_read_only.main(["--repository-root", str(repository_root)])
    return exit_code, standard_output.getvalue(), standard_error.getvalue()


class CheckCanReadOnlyTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.root = Path(self.temporary_directory.name)
        (self.root / "src" / "hardware" / "can").mkdir(parents=True)

    def write(self, name, text):
        (self.root / "src" / "hardware" / "can" / name).write_text(text, encoding="utf-8")

    def test_reading_code_passes(self):
        self.write("reader.cpp", "ssize_t count = ::read(fd, &frame, sizeof(frame));\nrewrite(x);\n// write(fd) in a comment\n")
        exit_code, output, _ = run_main(self.root)
        self.assertEqual(exit_code, check_can_read_only.EXIT_CODE_NO_FINDING, output)

    def test_every_transmit_call_fails(self):
        for call in ("write", "::write", "send", "sendto", "sendmsg", "writev", "pwrite"):
            with self.subTest(call=call):
                self.write("sender.cpp", f"int result = {call}(fd, buffer, 8);\n")
                exit_code, output, _ = run_main(self.root)
                self.assertEqual(exit_code, check_can_read_only.EXIT_CODE_FINDING)
                self.assertIn("src/hardware/can/sender.cpp:1: transmit call", output)

    def test_the_repository_itself_is_clean(self):
        exit_code, output, _ = run_main(check_can_read_only.DEFAULT_REPOSITORY_ROOT)
        self.assertEqual(exit_code, check_can_read_only.EXIT_CODE_NO_FINDING, output)

    def test_missing_folder_cannot_run(self):
        with tempfile.TemporaryDirectory() as empty:
            exit_code, _, error = run_main(Path(empty))
        self.assertEqual(exit_code, check_can_read_only.EXIT_CODE_CHECK_COULD_NOT_RUN)
        self.assertIn("is not a directory", error)


if __name__ == "__main__":
    unittest.main()
