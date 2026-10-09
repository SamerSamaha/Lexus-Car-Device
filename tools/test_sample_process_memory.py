#!/usr/bin/env python3
"""Tests for measure/sample_process_memory.py against a fake /proc tree."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "measure"))

import sample_process_memory as sampler  # noqa: E402


def add_process(proc_root, pid, parent_pid, name, rss_kilobytes, pss_kilobytes=None):
    folder = proc_root / str(pid)
    folder.mkdir(parents=True)
    (folder / "stat").write_text(f"{pid} ({name}) S {parent_pid} 1 1 0 -1\n", encoding="ascii")
    (folder / "status").write_text(f"Name:\t{name}\nVmRSS:\t{rss_kilobytes} kB\n", encoding="ascii")
    if pss_kilobytes is not None:
        (folder / "smaps_rollup").write_text(f"Rss: {rss_kilobytes} kB\nPss: {pss_kilobytes} kB\n", encoding="ascii")


class SampleProcessMemoryTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.proc = Path(self.temporary_directory.name) / "proc"
        self.proc.mkdir()
        (self.proc / "self").mkdir()
        add_process(self.proc, 1, 0, "systemd", 9000, 4000)
        add_process(self.proc, 100, 1, "lexus-hub", 50000, 30000)
        add_process(self.proc, 200, 100, "chromium", 120000, 80000)
        add_process(self.proc, 201, 200, "chromium", 90000, 30000)
        add_process(self.proc, 202, 201, "chromium (gpu)", 60000, 20000)
        add_process(self.proc, 300, 1, "bash", 4000, 2000)

    def test_tree_of_a_pid_includes_every_descendant_only(self):
        sample = sampler.sample_tree(self.proc, {200}, None)
        self.assertEqual(sample.process_count, 3)
        self.assertEqual(sample.rss_kilobytes, 120000 + 90000 + 60000)
        self.assertEqual(sample.pss_kilobytes, 80000 + 30000 + 20000)

    def test_roots_by_name_ignore_children_of_the_same_name(self):
        table = sampler.read_process_table(self.proc)
        self.assertEqual(sampler.roots_named(table, "chromium"), {200})
        self.assertEqual(table[202].name, "chromium (gpu)")
        self.assertEqual(sampler.sample_tree(self.proc, set(), "chromium").process_count, 3)

    def test_missing_pss_makes_the_pss_sum_unknown_not_low(self):
        add_process(self.proc, 203, 200, "chromium", 10000)
        sample = sampler.sample_tree(self.proc, {200}, None)
        self.assertEqual(sample.process_count, 4)
        self.assertIsNone(sample.pss_kilobytes)

    def test_session_writes_rows_and_a_summary(self):
        output = Path(self.temporary_directory.name) / "out" / "run.csv"
        clock = [0.0]
        rows, summary = sampler.run_session(
            self.proc, {100}, None, 10.0, 30.0, output,
            sleep=lambda seconds: clock.__setitem__(0, clock[0] + seconds),
            now=lambda: clock[0],
        )
        self.assertEqual(len(rows), 4)
        self.assertEqual(rows[-1]["elapsed_seconds"], "30.0")
        self.assertEqual(rows[0]["process_count"], "4")
        self.assertIn("pss_kilobytes: min 160000 median 160000 max 160000", summary)
        self.assertEqual(output.read_text(encoding="ascii").splitlines()[0], ",".join(sampler.CSV_COLUMNS))
        self.assertTrue(output.with_suffix(".summary.txt").exists())

    def test_without_pid_or_name_it_cannot_run(self):
        import contextlib
        import io

        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(sampler.main(["--label", "x"]), sampler.EXIT_CODE_COULD_NOT_RUN)


if __name__ == "__main__":
    unittest.main()
