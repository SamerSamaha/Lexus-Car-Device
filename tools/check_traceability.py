#!/usr/bin/env python3
"""Fail if an Approved or Provisional requirement has no tagged test and no measurement file.

Reads the requirement table in docs/requirements/REQUIREMENTS.md and the matrix in
docs/traceability/TRACEABILITY.md, collects every "Verifies: REQ-nnn" tag under tests/ and in
the tooling tests tools/test_*.py (the car-mode tools of LHU-044 are tested there), and checks
each requirement:

  * a matrix row whose Status starts with "Planned" is pending and does not fail the check;
  * every other row needs a tag in at least one test file, or a measurement or procedure file
    named in the row (a path under tools/measure/ or docs/test/) that exists;
  * a tag naming a requirement that is not in the requirements file fails;
  * a tagged requirement whose matrix row still says Planned fails, so the matrix stays current.

The full table is printed every time. Exit 0 clean, 1 finding, 2 could not run.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Sequence

EXIT_CODE_NO_FINDING = 0
EXIT_CODE_FINDING = 1
EXIT_CODE_CHECK_COULD_NOT_RUN = 2

DEFAULT_REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
REQUIREMENTS_RELATIVE_PATH = Path("docs/requirements/REQUIREMENTS.md")
TRACEABILITY_RELATIVE_PATH = Path("docs/traceability/TRACEABILITY.md")
TESTS_RELATIVE_PATH = Path("tests")

ENFORCED_STATUSES = ("Approved", "Provisional")
PLANNED_PREFIX = "Planned"
TEST_FILE_SUFFIXES = (".cpp", ".h", ".py", ".qml", ".sh")

REQUIREMENT_ID_PATTERN = re.compile(r"REQ-\d{3}")
TAG_LINE_PATTERN = re.compile(r"Verifies:\s*(REQ-\d{3}(?:\s*,\s*REQ-\d{3})*)")
EVIDENCE_PATH_PATTERN = re.compile(r"`((?:tools/measure|docs/test)/[^`]+)`")


class CheckCouldNotRunError(Exception):
    """The check itself failed: a file is missing or a table is malformed."""


@dataclass
class Requirement:
    requirement_id: str
    status: str


@dataclass
class MatrixRow:
    requirement_id: str
    test_column: str
    status_column: str


@dataclass
class RequirementReport:
    requirement_id: str
    requirement_status: str
    matrix_status: str
    tagged_files: List[str] = field(default_factory=list)
    evidence_files: List[str] = field(default_factory=list)
    result: str = ""
    reason: str = ""


def split_table_row(line: str) -> List[str]:
    """Return the cells of a markdown table row, without the outer pipes."""
    stripped = line.strip()
    if not stripped.startswith("|") or not stripped.endswith("|"):
        return []
    return [cell.strip() for cell in stripped[1:-1].split("|")]


def read_requirements(requirements_path: Path) -> List[Requirement]:
    """Read every REQ row of the requirements table: id in the first cell, status in the last."""
    try:
        lines = requirements_path.read_text(encoding="utf-8").splitlines()
    except OSError as read_error:
        raise CheckCouldNotRunError(f"cannot read {requirements_path}: {read_error}") from read_error
    requirements: List[Requirement] = []
    for line in lines:
        cells = split_table_row(line)
        if len(cells) >= 5 and REQUIREMENT_ID_PATTERN.fullmatch(cells[0]):
            requirements.append(Requirement(cells[0], cells[-1]))
    if not requirements:
        raise CheckCouldNotRunError(f"no requirement rows found in {requirements_path}")
    return requirements


def read_matrix(traceability_path: Path) -> Dict[str, MatrixRow]:
    """Read the matrix: id in the first cell, test in the fourth, status in the fifth."""
    try:
        lines = traceability_path.read_text(encoding="utf-8").splitlines()
    except OSError as read_error:
        raise CheckCouldNotRunError(f"cannot read {traceability_path}: {read_error}") from read_error
    rows: Dict[str, MatrixRow] = {}
    for line in lines:
        cells = split_table_row(line)
        if len(cells) >= 5 and REQUIREMENT_ID_PATTERN.fullmatch(cells[0]):
            rows[cells[0]] = MatrixRow(cells[0], cells[3], cells[4])
    if not rows:
        raise CheckCouldNotRunError(f"no matrix rows found in {traceability_path}")
    return rows


def collect_tags(tests_root: Path) -> Dict[str, List[str]]:
    """Map each tagged requirement id to the test files (relative to the repository) carrying it."""
    tagged_files: Dict[str, List[str]] = {}
    if not tests_root.is_dir():
        return tagged_files
    for test_file in sorted(tests_root.rglob("*")):
        if not test_file.is_file() or test_file.suffix not in TEST_FILE_SUFFIXES:
            continue
        try:
            text = test_file.read_text(encoding="utf-8", errors="replace")
        except OSError as read_error:
            raise CheckCouldNotRunError(f"cannot read {test_file}: {read_error}") from read_error
        relative_label = test_file.relative_to(tests_root.parent).as_posix()
        for tag_match in TAG_LINE_PATTERN.finditer(text):
            for requirement_id in REQUIREMENT_ID_PATTERN.findall(tag_match.group(1)):
                files = tagged_files.setdefault(requirement_id, [])
                if relative_label not in files:
                    files.append(relative_label)
    return tagged_files


def collect_tool_test_tags(tools_root: Path) -> Dict[str, List[str]]:
    """The same for the tooling tests directly in tools/ (test_*.py)."""
    tagged_files: Dict[str, List[str]] = {}
    if not tools_root.is_dir():
        return tagged_files
    for test_file in sorted(tools_root.glob("test_*.py")):
        try:
            text = test_file.read_text(encoding="utf-8", errors="replace")
        except OSError as read_error:
            raise CheckCouldNotRunError(f"cannot read {test_file}: {read_error}") from read_error
        relative_label = test_file.relative_to(tools_root.parent).as_posix()
        # Only comment lines: the tests of this checker carry tags as string data.
        comment_lines = "\n".join(
            line for line in text.splitlines() if line.lstrip().startswith("#")
        )
        for tag_match in TAG_LINE_PATTERN.finditer(comment_lines):
            for requirement_id in REQUIREMENT_ID_PATTERN.findall(tag_match.group(1)):
                files = tagged_files.setdefault(requirement_id, [])
                if relative_label not in files:
                    files.append(relative_label)
    return tagged_files


def existing_evidence_files(repository_root: Path, test_column: str) -> List[str]:
    """Return the measurement or procedure paths named in a matrix cell that exist in the tree."""
    existing: List[str] = []
    for evidence_path in EVIDENCE_PATH_PATTERN.findall(test_column):
        if (repository_root / evidence_path).is_file():
            existing.append(evidence_path)
    return existing


def build_reports(
    repository_root: Path,
    requirements: Sequence[Requirement],
    matrix: Dict[str, MatrixRow],
    tagged_files: Dict[str, List[str]],
) -> List[RequirementReport]:
    reports: List[RequirementReport] = []
    for requirement in requirements:
        row = matrix.get(requirement.requirement_id)
        report = RequirementReport(
            requirement.requirement_id,
            requirement.status,
            row.status_column if row else "(no matrix row)",
            list(tagged_files.get(requirement.requirement_id, [])),
        )
        if row is not None:
            report.evidence_files = existing_evidence_files(repository_root, row.test_column)
        if requirement.status not in ENFORCED_STATUSES:
            report.result, report.reason = "skipped", f"status {requirement.status}"
        elif row is None:
            report.result, report.reason = "FAIL", "no row in the traceability matrix"
        elif report.tagged_files and row.status_column.startswith(PLANNED_PREFIX):
            report.result, report.reason = "FAIL", "tagged test exists but the matrix row still says Planned"
        elif row.status_column.startswith(PLANNED_PREFIX):
            report.result, report.reason = "pending", "matrix row is Planned"
        elif report.tagged_files or report.evidence_files:
            report.result, report.reason = "pass", ""
        else:
            report.result, report.reason = "FAIL", "no tagged test and no existing measurement file"
        reports.append(report)
    return reports


def unknown_tags(requirements: Sequence[Requirement], tagged_files: Dict[str, List[str]]) -> List[str]:
    known_ids = {requirement.requirement_id for requirement in requirements}
    return [
        f"{requirement_id} in {', '.join(files)}"
        for requirement_id, files in sorted(tagged_files.items())
        if requirement_id not in known_ids
    ]


def print_reports(reports: Sequence[RequirementReport]) -> None:
    print(f"{'Requirement':<12}{'Status':<13}{'Result':<9}Evidence")
    for report in reports:
        evidence = ", ".join(report.tagged_files + report.evidence_files) or "-"
        detail = f" ({report.reason})" if report.reason else ""
        print(f"{report.requirement_id:<12}{report.requirement_status:<13}{report.result:<9}{evidence}{detail}")


def parse_arguments(argument_list: Optional[Sequence[str]]) -> argparse.Namespace:
    argument_parser = argparse.ArgumentParser(
        description="Fail if an Approved or Provisional requirement has no tagged test."
    )
    argument_parser.add_argument(
        "--repository-root",
        type=Path,
        default=DEFAULT_REPOSITORY_ROOT,
        help="repository to check (default: the repository this script is in)",
    )
    return argument_parser.parse_args(argument_list)


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    arguments = parse_arguments(argument_list)
    repository_root: Path = arguments.repository_root
    try:
        requirements = read_requirements(repository_root / REQUIREMENTS_RELATIVE_PATH)
        matrix = read_matrix(repository_root / TRACEABILITY_RELATIVE_PATH)
        tagged_files = collect_tags(repository_root / TESTS_RELATIVE_PATH)
        for requirement_id, files in collect_tool_test_tags(repository_root / "tools").items():
            tagged_files.setdefault(requirement_id, []).extend(files)
    except CheckCouldNotRunError as check_error:
        print(f"check_traceability: error: {check_error}", file=sys.stderr)
        return EXIT_CODE_CHECK_COULD_NOT_RUN

    reports = build_reports(repository_root, requirements, matrix, tagged_files)
    print_reports(reports)
    failures = [report for report in reports if report.result == "FAIL"]
    unknown = unknown_tags(requirements, tagged_files)
    for unknown_tag in unknown:
        print(f"unknown requirement in a test tag: {unknown_tag}")
    passed = sum(1 for report in reports if report.result == "pass")
    pending = sum(1 for report in reports if report.result == "pending")
    print(
        f"check_traceability: {len(reports)} requirements, {passed} with evidence, "
        f"{pending} pending, {len(failures)} failing, {len(unknown)} unknown tags"
    )
    if failures or unknown:
        print("check_traceability: FAILED.", file=sys.stderr)
        return EXIT_CODE_FINDING
    return EXIT_CODE_NO_FINDING


if __name__ == "__main__":
    sys.exit(main())
