#!/usr/bin/env python3
"""Fail if the HMI reaches past the view models (REQ-011).

Under src/hmi/, and under src/hub/viewmodels/ and src/hub/qml/ when they exist:
  * no C++ file may include a header from lexus_head_unit/hardware/;
  * the only lexus_head_unit/service/ headers allowed are the value types the view models
    display: signal_id.h, signal_sample.h, signal_definition.h, connection_state_machine.h;
  * QML files may import only Qt modules (QtQuick, QtQuick.*, QtQml, QtQml.*, QtTest) and the
    project's own modules LexusHeadUnit and LexusHub.

Exit 0 clean, 1 finding, 2 could not run.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import List, Optional, Sequence

EXIT_CODE_NO_FINDING = 0
EXIT_CODE_FINDING = 1
EXIT_CODE_CHECK_COULD_NOT_RUN = 2

DEFAULT_REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
HMI_RELATIVE_PATH = Path("src/hmi")
OPTIONAL_RELATIVE_PATHS = (Path("src/hub/viewmodels"), Path("src/hub/qml"))
CPP_SUFFIXES = (".h", ".hpp", ".cpp")
ALLOWED_SERVICE_HEADERS = (
    "lexus_head_unit/service/signal_id.h",
    "lexus_head_unit/service/signal_sample.h",
    "lexus_head_unit/service/signal_definition.h",
    "lexus_head_unit/service/connection_state_machine.h",
)
ALLOWED_QML_IMPORT_PREFIXES = ("QtQuick", "QtQml", "LexusHeadUnit", "LexusHub", "QtTest")

INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
IMPORT_PATTERN = re.compile(r"^\s*import\s+([A-Za-z0-9_.]+|\"[^\"]*\")")


def check_cpp_file(path: Path, label: str) -> List[str]:
    findings: List[str] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1):
        match = INCLUDE_PATTERN.match(line)
        if not match:
            continue
        header = match.group(1)
        if header.startswith("lexus_head_unit/hardware/"):
            findings.append(f"{label}:{line_number}: includes a hardware header: {header}")
        elif header.startswith("lexus_head_unit/service/") and header not in ALLOWED_SERVICE_HEADERS:
            findings.append(f"{label}:{line_number}: includes a service header that is not a value type: {header}")
    return findings


def check_qml_file(path: Path, label: str) -> List[str]:
    findings: List[str] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1):
        match = IMPORT_PATTERN.match(line)
        if not match:
            continue
        module = match.group(1)
        if module.startswith('"') or not module.startswith(ALLOWED_QML_IMPORT_PREFIXES):
            findings.append(f"{label}:{line_number}: imports outside Qt and the project modules: {module}")
    return findings


def check_hmi(repository_root: Path) -> List[str]:
    hmi_root = repository_root / HMI_RELATIVE_PATH
    if not hmi_root.is_dir():
        raise FileNotFoundError(f"{hmi_root} is not a directory")
    roots = [hmi_root] + [repository_root / relative for relative in OPTIONAL_RELATIVE_PATHS
                          if (repository_root / relative).is_dir()]
    paths = sorted(path for root in roots for path in root.rglob("*"))
    findings: List[str] = []
    for path in paths:
        if not path.is_file():
            continue
        label = path.relative_to(repository_root).as_posix()
        if path.suffix in CPP_SUFFIXES:
            findings.extend(check_cpp_file(path, label))
        elif path.suffix == ".qml":
            findings.extend(check_qml_file(path, label))
    return findings


def parse_arguments(argument_list: Optional[Sequence[str]]) -> argparse.Namespace:
    argument_parser = argparse.ArgumentParser(description="Fail if the HMI reaches past the view models.")
    argument_parser.add_argument(
        "--repository-root",
        type=Path,
        default=DEFAULT_REPOSITORY_ROOT,
        help="repository to check (default: the repository this script is in)",
    )
    return argument_parser.parse_args(argument_list)


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    arguments = parse_arguments(argument_list)
    try:
        findings = check_hmi(arguments.repository_root)
    except (FileNotFoundError, OSError) as error:
        print(f"check_hmi_includes: error: {error}", file=sys.stderr)
        return EXIT_CODE_CHECK_COULD_NOT_RUN
    for finding in findings:
        print(finding)
    print(f"check_hmi_includes: {len(findings)} findings")
    if findings:
        print("check_hmi_includes: FAILED.", file=sys.stderr)
        return EXIT_CODE_FINDING
    return EXIT_CODE_NO_FINDING


if __name__ == "__main__":
    sys.exit(main())
