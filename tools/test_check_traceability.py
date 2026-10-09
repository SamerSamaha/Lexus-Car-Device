#!/usr/bin/env python3
"""Tests for check_traceability.py.

Run: python3 -m unittest discover --start-directory tools --pattern "test_*.py"
"""

import contextlib
import io
import tempfile
import unittest
from pathlib import Path

import check_traceability

REQUIREMENTS_HEADER = "| ID | Requirement | Acceptance criterion | Verification | Status |\n|---|---|---|---|---|\n"
MATRIX_HEADER = (
    "| Requirement | Design element | Ticket | Test or measurement | Status |\n|---|---|---|---|---|\n"
)


def run_main(repository_root):
    standard_output = io.StringIO()
    standard_error = io.StringIO()
    with contextlib.redirect_stdout(standard_output), contextlib.redirect_stderr(standard_error):
        exit_code = check_traceability.main(["--repository-root", str(repository_root)])
    return exit_code, standard_output.getvalue(), standard_error.getvalue()


class FakeRepository:
    def __init__(self, root):
        self.root = Path(root)
        (self.root / "docs" / "requirements").mkdir(parents=True)
        (self.root / "docs" / "traceability").mkdir(parents=True)
        (self.root / "tests" / "unit").mkdir(parents=True)

    def write_requirements(self, rows):
        text = REQUIREMENTS_HEADER + "".join(
            f"| {requirement_id} | does something | measurable | Unit test | {status} |\n"
            for requirement_id, status in rows
        )
        (self.root / "docs" / "requirements" / "REQUIREMENTS.md").write_text(text, encoding="utf-8")

    def write_matrix(self, rows):
        text = MATRIX_HEADER + "".join(
            f"| {requirement_id} | `Thing` | LHU-999 | {test} | {status} |\n"
            for requirement_id, test, status in rows
        )
        (self.root / "docs" / "traceability" / "TRACEABILITY.md").write_text(text, encoding="utf-8")

    def write_test(self, relative_name, tag_line):
        path = self.root / "tests" / relative_name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f"{tag_line}\n\nint main() {{ return 0; }}\n", encoding="utf-8")

    def write_file(self, relative_path, text="x\n"):
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


class CheckTraceabilityTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.repository = FakeRepository(self.temporary_directory.name)

    def test_tagged_requirement_passes(self):
        self.repository.write_requirements([("REQ-001", "Approved")])
        self.repository.write_matrix([("REQ-001", "`tests/unit/a_test.cpp`", "Tagged since abc1234")])
        self.repository.write_test("unit/a_test.cpp", "// Verifies: REQ-001")
        exit_code, output, _ = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_NO_FINDING, output)
        self.assertIn("1 with evidence, 0 pending, 0 failing", output)

    def test_approved_requirement_without_tag_fails(self):
        self.repository.write_requirements([("REQ-001", "Approved")])
        self.repository.write_matrix([("REQ-001", "`tests/unit/a_test.cpp`", "Tagged since abc1234")])
        exit_code, output, _ = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_FINDING)
        self.assertIn("no tagged test and no existing measurement file", output)

    def test_planned_row_is_pending_not_failing(self):
        self.repository.write_requirements([("REQ-001", "Approved"), ("REQ-002", "Provisional")])
        self.repository.write_matrix(
            [("REQ-001", "`tests/unit/a_test.cpp`", "Planned"), ("REQ-002", "x", "Planned, target provisional")]
        )
        exit_code, output, _ = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_NO_FINDING, output)
        self.assertIn("0 with evidence, 2 pending, 0 failing", output)

    def test_tagged_test_with_planned_row_fails_so_the_matrix_is_updated(self):
        self.repository.write_requirements([("REQ-001", "Approved")])
        self.repository.write_matrix([("REQ-001", "`tests/unit/a_test.cpp`", "Planned")])
        self.repository.write_test("unit/a_test.cpp", "// Verifies: REQ-001")
        exit_code, output, _ = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_FINDING)
        self.assertIn("matrix row still says Planned", output)

    def test_unknown_requirement_in_a_tag_fails(self):
        self.repository.write_requirements([("REQ-001", "Approved")])
        self.repository.write_matrix([("REQ-001", "`tests/unit/a_test.cpp`", "Tagged")])
        self.repository.write_test("unit/a_test.cpp", "// Verifies: REQ-001, REQ-077")
        exit_code, output, _ = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_FINDING)
        self.assertIn("unknown requirement in a test tag: REQ-077", output)

    def test_measurement_file_named_in_the_matrix_counts_when_it_exists(self):
        self.repository.write_requirements([("REQ-009", "Approved")])
        self.repository.write_matrix(
            [("REQ-009", "`tools/measure/measure_latency.py`; raw data", "Script exists")]
        )
        exit_code, _, _ = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_FINDING)
        self.repository.write_file("tools/measure/measure_latency.py")
        exit_code, output, _ = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_NO_FINDING, output)

    def test_withdrawn_requirement_is_skipped(self):
        self.repository.write_requirements([("REQ-001", "Withdrawn")])
        self.repository.write_matrix([("REQ-001", "x", "Withdrawn")])
        exit_code, output, _ = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_NO_FINDING)
        self.assertIn("skipped", output)

    def test_requirement_missing_from_the_matrix_fails(self):
        self.repository.write_requirements([("REQ-001", "Approved"), ("REQ-002", "Approved")])
        self.repository.write_matrix([("REQ-001", "`tests/unit/a_test.cpp`", "Tagged")])
        self.repository.write_test("unit/a_test.cpp", "// Verifies: REQ-001")
        exit_code, output, _ = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_FINDING)
        self.assertIn("no row in the traceability matrix", output)

    def test_tags_in_python_and_qml_files_are_collected(self):
        self.repository.write_requirements([("REQ-001", "Approved"), ("REQ-002", "Approved")])
        self.repository.write_matrix([("REQ-001", "x", "Tagged"), ("REQ-002", "x", "Tagged")])
        self.repository.write_test("scenarios/drop_test.py", "# Verifies: REQ-001")
        self.repository.write_test("hmi/tile_test.qml", "// Verifies: REQ-002")
        exit_code, output, _ = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_NO_FINDING, output)
        self.assertIn("tests/scenarios/drop_test.py", output)
        self.assertIn("tests/hmi/tile_test.qml", output)

    def test_missing_requirements_file_cannot_run(self):
        self.repository.write_matrix([("REQ-001", "x", "Tagged")])
        exit_code, _, error = run_main(self.repository.root)
        self.assertEqual(exit_code, check_traceability.EXIT_CODE_CHECK_COULD_NOT_RUN)
        self.assertIn("cannot read", error)


if __name__ == "__main__":
    unittest.main()
