#!/usr/bin/env python3
"""Fail if a tracked file contains a VIN-shaped or Bluetooth-address-shaped string.

The repository is public. The vehicle's VIN and any Bluetooth address must
never be committed. This script scans the name and the content of every file
tracked by git and reports:

  VIN-shaped string
      A standalone run of exactly 17 characters from the VIN alphabet (digits
      and capital letters except I, O and Q) that contains at least one letter
      and at least one digit.

  Bluetooth-address-shaped string
      Six groups of two hexadecimal digits separated by colons or hyphens.

Documented false positives are listed in private_data_allowlist.txt, each with
the reason it is safe.

Known limits: the check works on shapes, so it does not find a VIN written in
lower case or a VIN encoded as hexadecimal bytes, which is how an OBD adapter
returns it. Raw recordings therefore stay in the ignored local_recordings/
folder until they have been scrubbed.

Usage:
    python3 tools/check_private_data.py                  scan every tracked file
    python3 tools/check_private_data.py FILE [FILE ...]  scan only these files
    python3 tools/check_private_data.py --show-matches   print matches unmasked

Run it before every commit: once a commit has been pushed, its content is public.

Exit code: 0 no finding, 1 at least one finding, 2 the check could not run.
Python standard library only.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

EXIT_CODE_NO_FINDING = 0
EXIT_CODE_FINDING = 1
EXIT_CODE_CHECK_COULD_NOT_RUN = 2

KIND_VIN_SHAPED = "VIN-shaped string"
KIND_BLUETOOTH_ADDRESS_SHAPED = "Bluetooth-address-shaped string"

# "Standalone" means the character before and the character after the run are
# not letters or digits. An underscore or a hyphen therefore ends a run, so a
# VIN inside a file name such as drive_<VIN>_morning.csv is still found.
VIN_SHAPED_PATTERN = re.compile(r"(?<![A-Za-z0-9])[A-HJ-NPR-Z0-9]{17}(?![A-Za-z0-9])")

# A longer run of groups (for example seven) is reported too: the first six
# groups match. Reporting too much is the safe direction for this check.
BLUETOOTH_ADDRESS_SHAPED_PATTERN = re.compile(
    r"(?<![0-9A-Fa-f])[0-9A-Fa-f]{2}(?:[:-][0-9A-Fa-f]{2}){5}(?![0-9A-Fa-f])"
)

ALLOWLIST_SEPARATOR = "|"
NUMBER_OF_BYTES_INSPECTED_FOR_BINARY_DETECTION = 8192
VIN_CHARACTERS_SHOWN_WHEN_MASKED = 3
BLUETOOTH_ADDRESS_CHARACTERS_SHOWN_WHEN_MASKED = 8
LINE_NUMBER_OF_FILE_NAME = 0

DEFAULT_REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ALLOWLIST_PATH = Path(__file__).resolve().parent / "private_data_allowlist.txt"


class CheckCouldNotRunError(Exception):
    """The check itself failed, for example git is missing or the allowlist is malformed."""


@dataclass(frozen=True)
class PrivateDataFinding:
    """One VIN-shaped or Bluetooth-address-shaped string found in a file."""

    file_label: str
    line_number: int  # 1-based; LINE_NUMBER_OF_FILE_NAME means the file name itself
    kind: str
    matched_text: str


def find_shaped_strings(text: str) -> List[Tuple[str, str]]:
    """Return (kind, matched text) for every shaped string in one piece of text."""
    shaped_strings: List[Tuple[str, str]] = []
    for vin_match in VIN_SHAPED_PATTERN.finditer(text):
        candidate = vin_match.group(0)
        contains_letter = any(character.isalpha() for character in candidate)
        contains_digit = any(character.isdigit() for character in candidate)
        if contains_letter and contains_digit:
            shaped_strings.append((KIND_VIN_SHAPED, candidate))
    for address_match in BLUETOOTH_ADDRESS_SHAPED_PATTERN.finditer(text):
        shaped_strings.append((KIND_BLUETOOTH_ADDRESS_SHAPED, address_match.group(0)))
    return shaped_strings


def scan_text(text: str, file_label: str) -> List[PrivateDataFinding]:
    """Scan the content of one file, line by line."""
    findings: List[PrivateDataFinding] = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        for kind, matched_text in find_shaped_strings(line):
            findings.append(PrivateDataFinding(file_label, line_number, kind, matched_text))
    return findings


def scan_file_name(file_label: str) -> List[PrivateDataFinding]:
    """Scan the path of a file, because a file name is public too."""
    return [
        PrivateDataFinding(file_label, LINE_NUMBER_OF_FILE_NAME, kind, matched_text)
        for kind, matched_text in find_shaped_strings(file_label)
    ]


def is_binary(content: bytes) -> bool:
    """Treat a file as binary if a NUL byte appears near its start (the rule git uses)."""
    return b"\x00" in content[:NUMBER_OF_BYTES_INSPECTED_FOR_BINARY_DETECTION]


def load_allowlist(allowlist_path: Path) -> Dict[str, str]:
    """Read the allowlist: exact matched text -> reason it is safe.

    Each entry is one line: the exact text, a "|", then the reason. Blank lines
    and lines starting with "#" are ignored. An entry without a reason is an error.
    """
    reason_by_allowed_text: Dict[str, str] = {}
    try:
        allowlist_lines = allowlist_path.read_text(encoding="utf-8").splitlines()
    except OSError as read_error:
        raise CheckCouldNotRunError(
            f"cannot read allowlist {allowlist_path}: {read_error}"
        ) from read_error
    for line_number, line in enumerate(allowlist_lines, start=1):
        stripped_line = line.strip()
        if not stripped_line or stripped_line.startswith("#"):
            continue
        allowed_text, separator, reason = stripped_line.partition(ALLOWLIST_SEPARATOR)
        allowed_text = allowed_text.strip()
        reason = reason.strip()
        if not separator or not allowed_text or not reason:
            raise CheckCouldNotRunError(
                f"{allowlist_path}:{line_number}: expected '<exact text> | <reason>'"
            )
        reason_by_allowed_text[allowed_text] = reason
    return reason_by_allowed_text


def list_tracked_files(repository_root: Path) -> List[str]:
    """Return the path of every file tracked by git, relative to the repository root."""
    try:
        completed_process = subprocess.run(
            ["git", "-C", str(repository_root), "ls-files", "-z"],
            check=True,
            capture_output=True,
        )
    except FileNotFoundError as missing_git_error:
        raise CheckCouldNotRunError("git was not found on PATH") from missing_git_error
    except subprocess.CalledProcessError as git_error:
        error_text = git_error.stderr.decode("utf-8", errors="replace").strip()
        raise CheckCouldNotRunError(f"git ls-files failed: {error_text}") from git_error
    output_text = completed_process.stdout.decode("utf-8", errors="replace")
    return [relative_path for relative_path in output_text.split("\x00") if relative_path]


def mask_matched_text(kind: str, matched_text: str) -> str:
    """Hide most of a match, so the report does not repeat the private value.

    The part left visible is the manufacturer prefix, which is not private:
    the first 3 characters of a VIN, the first 3 groups of a Bluetooth address.
    """
    if kind == KIND_VIN_SHAPED:
        number_of_characters_shown = VIN_CHARACTERS_SHOWN_WHEN_MASKED
    else:
        number_of_characters_shown = BLUETOOTH_ADDRESS_CHARACTERS_SHOWN_WHEN_MASKED
    visible_part = matched_text[:number_of_characters_shown]
    hidden_part = matched_text[number_of_characters_shown:]
    masked_hidden_part = "".join(
        "*" if character.isalnum() else character for character in hidden_part
    )
    return visible_part + masked_hidden_part


def format_finding(finding: PrivateDataFinding, show_matches: bool) -> str:
    """Build the report line for one finding: file, line, kind and the match."""
    if show_matches:
        displayed_text = finding.matched_text
    else:
        displayed_text = mask_matched_text(finding.kind, finding.matched_text)
    if finding.line_number == LINE_NUMBER_OF_FILE_NAME:
        location = f"{finding.file_label}: (file name)"
    else:
        location = f"{finding.file_label}:{finding.line_number}"
    return f"{location}: {finding.kind}: {displayed_text}"


def scan_files(
    repository_root: Path,
    relative_paths: Sequence[str],
    reason_by_allowed_text: Dict[str, str],
) -> Tuple[List[PrivateDataFinding], List[str]]:
    """Scan the given files. Return (findings not on the allowlist, files skipped with a reason)."""
    findings: List[PrivateDataFinding] = []
    skipped_files: List[str] = []
    for relative_path in relative_paths:
        candidate_findings = scan_file_name(relative_path)
        file_path = repository_root / relative_path
        if not file_path.is_file():
            skipped_files.append(f"{relative_path} (not a regular file in the working tree)")
        else:
            try:
                content = file_path.read_bytes()
            except OSError as read_error:
                raise CheckCouldNotRunError(
                    f"cannot read {relative_path}: {read_error}"
                ) from read_error
            if is_binary(content):
                skipped_files.append(f"{relative_path} (binary content)")
            else:
                # Undecodable bytes are replaced, not dropped, so the rest of
                # the file is still scanned.
                text = content.decode("utf-8", errors="replace")
                candidate_findings.extend(scan_text(text, relative_path))
        for candidate_finding in candidate_findings:
            if candidate_finding.matched_text not in reason_by_allowed_text:
                findings.append(candidate_finding)
    return findings, skipped_files


def parse_arguments(argument_list: Optional[Sequence[str]]) -> argparse.Namespace:
    argument_parser = argparse.ArgumentParser(
        description="Fail if a tracked file contains a VIN-shaped or "
        "Bluetooth-address-shaped string."
    )
    argument_parser.add_argument(
        "files",
        nargs="*",
        help="files to scan, relative to the repository root (default: every tracked file)",
    )
    argument_parser.add_argument(
        "--repository-root",
        type=Path,
        default=DEFAULT_REPOSITORY_ROOT,
        help="repository to scan (default: the repository this script is in)",
    )
    argument_parser.add_argument(
        "--allowlist",
        type=Path,
        default=DEFAULT_ALLOWLIST_PATH,
        help="allowlist file (default: private_data_allowlist.txt next to this script)",
    )
    argument_parser.add_argument(
        "--show-matches",
        action="store_true",
        help="print each match in full instead of masked; for local use only, never in CI",
    )
    return argument_parser.parse_args(argument_list)


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    arguments = parse_arguments(argument_list)
    try:
        reason_by_allowed_text = load_allowlist(arguments.allowlist)
        if arguments.files:
            relative_paths = list(arguments.files)
        else:
            relative_paths = list_tracked_files(arguments.repository_root)
        findings, skipped_files = scan_files(
            arguments.repository_root, relative_paths, reason_by_allowed_text
        )
    except CheckCouldNotRunError as check_error:
        print(f"check_private_data: error: {check_error}", file=sys.stderr)
        return EXIT_CODE_CHECK_COULD_NOT_RUN

    for skipped_file in skipped_files:
        print(f"skipped: {skipped_file}")
    for finding in findings:
        print(format_finding(finding, arguments.show_matches))
    number_of_files_scanned = len(relative_paths) - len(skipped_files)
    print(
        f"check_private_data: {number_of_files_scanned} files scanned, "
        f"{len(skipped_files)} skipped, {len(findings)} findings"
    )
    if findings:
        print(
            "check_private_data: FAILED. Remove the private data, or add a documented "
            "false positive to the allowlist with its reason.",
            file=sys.stderr,
        )
        return EXIT_CODE_FINDING
    return EXIT_CODE_NO_FINDING


if __name__ == "__main__":
    sys.exit(main())
