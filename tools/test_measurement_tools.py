#!/usr/bin/env python3
"""Tests for measure/measure_latency.py, measure_boot_time.py and measure_memory.py."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "measure"))

import measure_boot_time  # noqa: E402
import measure_latency  # noqa: E402
import measure_memory  # noqa: E402

ANALYZE = "Startup finished in 2.512s (kernel) + 1min 4.250s (userspace) = 1min 6.762s\ngraphical.target reached after 1min 4.1s in userspace.\n"


class MeasureLatencyTest(unittest.TestCase):
    def test_nearest_rank_percentiles(self):
        values = list(range(1, 101))
        self.assertEqual(measure_latency.percentile(values, 0.5), 50)
        self.assertEqual(measure_latency.percentile(values, 0.95), 95)
        self.assertEqual(measure_latency.percentile(values, 0.99), 99)
        self.assertEqual(measure_latency.percentile([7], 0.95), 7)

    def test_summary_splits_the_figure_and_judges_only_with_1000_samples(self):
        rows = [{"sample_ms": 0, "arrival_ms": 2, "frame_ms": 10 + (index % 100)} for index in range(1000)]
        summary = measure_latency.summarise(rows)
        self.assertIn("samples: 1000", summary)
        self.assertIn("sample to screen (ms): min 10 median 59 p95 104 p99 108 max 109", summary)
        self.assertIn("sample to view model (ms): min 2 median 2", summary)
        self.assertIn("REQ-009: pass (p95 104 ms, target 200 ms)", summary)
        self.assertIn("too few samples (10 of 1000)", measure_latency.summarise(rows[:10]))
        slow = [{"sample_ms": 0, "arrival_ms": 0, "frame_ms": 300} for _ in range(1000)]
        self.assertIn("REQ-009: fail", measure_latency.summarise(slow))

    def test_reads_the_probe_csv(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "latency.csv"
            path.write_text("signal,sample_ms,arrival_ms,frame_ms\nVehicleSpeed,100,101,106\n", encoding="ascii")
            self.assertEqual(measure_latency.read_rows(path), [{"sample_ms": 100, "arrival_ms": 101, "frame_ms": 106}])


class MeasureBootTimeTest(unittest.TestCase):
    def test_parses_the_mark_and_systemd_analyze(self):
        self.assertEqual(measure_boot_time.parse_mark("first_frame_boottime_ms=12345\n"), 12345)
        with self.assertRaises(ValueError):
            measure_boot_time.parse_mark("nothing")
        times = measure_boot_time.parse_systemd_analyze(ANALYZE)
        self.assertAlmostEqual(times["kernel"], 2.512)
        self.assertAlmostEqual(times["userspace"], 64.25)
        self.assertAlmostEqual(measure_boot_time.parse_systemd_analyze("Startup finished in 850ms (kernel) + 7.1s (userspace)")["kernel"], 0.85)

    def test_records_each_boot_once_and_summarises(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "boots.csv"
            for boot in range(10):
                measure_boot_time.record(f"first_frame_boottime_ms={11000 + boot * 100}\n", ANALYZE, f"boot-{boot}", output)
            with self.assertRaises(FileExistsError):
                measure_boot_time.record("first_frame_boottime_ms=1\n", ANALYZE, "boot-3", output)
            rows = measure_boot_time.read_rows(output)
            self.assertEqual(len(rows), 10)
            self.assertEqual(rows[0]["kernel_seconds"], "2.512")
            summary = measure_boot_time.summarise(rows)
            self.assertIn("min 11000 median 11450 max 11900", summary)
            self.assertIn("every boot within the target, worst 11900 ms", summary)
            self.assertIn("too few boots (3 of 10)", measure_boot_time.summarise(rows[:3]))


class MeasureMemoryTest(unittest.TestCase):
    def test_process_arguments(self):
        self.assertEqual(measure_memory.parse_process_arguments(["hub=lexus-hub"]), [("hub", "lexus-hub")])
        for bad in ("hub", "=x", "hub="):
            with self.assertRaises(ValueError):
                measure_memory.parse_process_arguments([bad])

    def test_session_per_label_and_growth_from_minute_5(self):
        with tempfile.TemporaryDirectory() as folder:
            proc = Path(folder) / "proc"
            proc.mkdir()
            pss = {"value": 100000}

            def add(pid, parent, name):
                directory = proc / str(pid)
                directory.mkdir(exist_ok=True)
                (directory / "stat").write_text(f"{pid} ({name}) S {parent} 1\n", encoding="ascii")
                (directory / "status").write_text(f"VmRSS:\t{pss['value'] * 2} kB\n", encoding="ascii")
                (directory / "smaps_rollup").write_text(f"Pss: {pss['value']} kB\n", encoding="ascii")

            add(10, 1, "lexus-hub")
            clock = [0.0]

            def sleep(seconds):
                clock[0] += seconds
                pss["value"] += 1000
                add(10, 1, "lexus-hub")

            rows, summary = measure_memory.run_session(
                proc, [("hub", "lexus-hub"), ("browser", "chromium")], 300.0, 1800.0,
                Path(folder) / "out.csv", sleep=sleep, now=lambda: clock[0])
            self.assertEqual(len(rows), 7)
            self.assertEqual(rows[0]["hub_processes"], "1")
            self.assertEqual(rows[0]["browser_pss_kb"], "")
            self.assertIn("hub (lexus-hub) PSS kB: min 100000 median 103000 max 106000", summary)
            self.assertIn("growth from minute 5: +4.95 % (under 5 %)", summary)
            self.assertIn("browser (chromium): no PSS values", summary)


if __name__ == "__main__":
    unittest.main()
