#!/usr/bin/env python3
"""Unit tests for check_private_data.py.

Run from the repository root:
    python3 -m unittest discover --start-directory tools --pattern "test_*.py" --verbose

This file is itself scanned by the check. The only shaped strings written out
in full here are the two documented samples on the allowlist. Every other test
value is built from them at run time, so it never appears as text in this file.
"""

import contextlib
import io
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import check_private_data

# Hypothetical VIN from the check-digit example in the Wikipedia article
# "Vehicle identification number". On the allowlist.
SAMPLE_VIN = "1M8GDM9AXKP042788"
# Address from the range RFC 7042 reserves for documentation. On the allowlist.
SAMPLE_BLUETOOTH_ADDRESS = "00:00:5E:00:53:01"

# Shaped strings that are NOT on the allowlist, built at run time.
UNLISTED_VIN = SAMPLE_VIN[::-1]
UNLISTED_BLUETOOTH_ADDRESS = ":".join(reversed(SAMPLE_BLUETOOTH_ADDRESS.split(":")))

REAL_ALLOWLIST_PATH = Path(check_private_data.__file__).resolve().parent / (
    "private_data_allowlist.txt"
)


def kinds_found(text):
    return [kind for kind, _matched_text in check_private_data.find_shaped_strings(text)]


def run_main(argument_list):
    """Run main() and return (exit code, standard output, standard error)."""
    standard_output = io.StringIO()
    standard_error = io.StringIO()
    with contextlib.redirect_stdout(standard_output), contextlib.redirect_stderr(standard_error):
        exit_code = check_private_data.main(argument_list)
    return exit_code, standard_output.getvalue(), standard_error.getvalue()


class VinShapedStringTest(unittest.TestCase):
    def test_sample_vin_alone_is_found(self):
        self.assertEqual(
            check_private_data.find_shaped_strings(SAMPLE_VIN),
            [(check_private_data.KIND_VIN_SHAPED, SAMPLE_VIN)],
        )

    def test_vin_inside_a_sentence_is_found(self):
        self.assertEqual(
            kinds_found(f"The car reported {SAMPLE_VIN} on the first request."),
            [check_private_data.KIND_VIN_SHAPED],
        )

    def test_vin_next_to_punctuation_or_underscore_is_found(self):
        for surrounding in ('"{}"', "({})", "vin={};", "drive_{}_morning.csv", "<{}>", "-{}-"):
            with self.subTest(surrounding=surrounding):
                self.assertEqual(
                    kinds_found(surrounding.format(SAMPLE_VIN)),
                    [check_private_data.KIND_VIN_SHAPED],
                )

    def test_two_vins_on_one_line_are_both_found(self):
        self.assertEqual(len(kinds_found(f"{SAMPLE_VIN} {UNLISTED_VIN}")), 2)

    def test_run_of_16_characters_is_not_found(self):
        self.assertEqual(kinds_found(SAMPLE_VIN[:16]), [])

    def test_run_of_18_characters_is_not_found(self):
        self.assertEqual(kinds_found(SAMPLE_VIN + "7"), [])
        self.assertEqual(kinds_found("7" + SAMPLE_VIN), [])

    def test_vin_touching_a_lower_case_letter_is_not_standalone(self):
        self.assertEqual(kinds_found(SAMPLE_VIN + "x"), [])
        self.assertEqual(kinds_found("x" + SAMPLE_VIN), [])

    def test_letters_outside_the_vin_alphabet_are_not_found(self):
        for excluded_letter in "IOQ":
            with self.subTest(excluded_letter=excluded_letter):
                candidate = SAMPLE_VIN[:8] + excluded_letter + SAMPLE_VIN[9:]
                self.assertEqual(len(candidate), 17)
                self.assertEqual(kinds_found(candidate), [])

    def test_17_digits_without_a_letter_are_not_found(self):
        self.assertEqual(kinds_found("1" * 17), [])

    def test_17_letters_without_a_digit_are_not_found(self):
        self.assertEqual(kinds_found("A" * 17), [])

    def test_lower_case_vin_is_not_found(self):
        # Documents a known limit of the shape check, stated in the script.
        self.assertEqual(kinds_found(SAMPLE_VIN.lower()), [])


class BluetoothAddressShapedStringTest(unittest.TestCase):
    def test_colon_separated_address_is_found(self):
        self.assertEqual(
            check_private_data.find_shaped_strings(SAMPLE_BLUETOOTH_ADDRESS),
            [(check_private_data.KIND_BLUETOOTH_ADDRESS_SHAPED, SAMPLE_BLUETOOTH_ADDRESS)],
        )

    def test_hyphen_separated_address_is_found(self):
        self.assertEqual(
            kinds_found(SAMPLE_BLUETOOTH_ADDRESS.replace(":", "-")),
            [check_private_data.KIND_BLUETOOTH_ADDRESS_SHAPED],
        )

    def test_lower_case_address_is_found(self):
        self.assertEqual(
            kinds_found(SAMPLE_BLUETOOTH_ADDRESS.lower()),
            [check_private_data.KIND_BLUETOOTH_ADDRESS_SHAPED],
        )

    def test_address_inside_a_command_is_found(self):
        self.assertEqual(
            kinds_found(f"pair {SAMPLE_BLUETOOTH_ADDRESS} then trust it"),
            [check_private_data.KIND_BLUETOOTH_ADDRESS_SHAPED],
        )

    def test_five_groups_are_not_found(self):
        five_groups = SAMPLE_BLUETOOTH_ADDRESS[: len("00:00:5E:00:53")]
        self.assertEqual(five_groups.count(":"), 4)
        self.assertEqual(kinds_found(five_groups), [])

    def test_seven_groups_are_reported(self):
        # Reporting too much is the safe direction: the first six groups match.
        self.assertEqual(
            check_private_data.find_shaped_strings(SAMPLE_BLUETOOTH_ADDRESS + ":02"),
            [(check_private_data.KIND_BLUETOOTH_ADDRESS_SHAPED, SAMPLE_BLUETOOTH_ADDRESS)],
        )

    def test_group_that_is_not_hexadecimal_is_not_found(self):
        self.assertEqual(kinds_found("GG" + SAMPLE_BLUETOOTH_ADDRESS[2:]), [])

    def test_groups_of_three_digits_are_not_found(self):
        self.assertEqual(kinds_found("0" + SAMPLE_BLUETOOTH_ADDRESS), [])
        self.assertEqual(kinds_found(SAMPLE_BLUETOOTH_ADDRESS + "0"), [])

    def test_time_of_day_is_not_found(self):
        self.assertEqual(kinds_found("started at 12:34:56 and ended at 13:00:01"), [])


class ScanTextTest(unittest.TestCase):
    def test_line_numbers_start_at_1(self):
        text = f"first line\nsecond line {UNLISTED_VIN}\nthird line\n{UNLISTED_BLUETOOTH_ADDRESS}\n"
        findings = check_private_data.scan_text(text, "notes.txt")
        self.assertEqual(
            [(finding.line_number, finding.kind) for finding in findings],
            [
                (2, check_private_data.KIND_VIN_SHAPED),
                (4, check_private_data.KIND_BLUETOOTH_ADDRESS_SHAPED),
            ],
        )
        self.assertEqual(findings[0].file_label, "notes.txt")
        self.assertEqual(findings[0].matched_text, UNLISTED_VIN)

    def test_windows_line_endings_do_not_hide_a_match(self):
        findings = check_private_data.scan_text(f"one\r\n{UNLISTED_VIN}\r\n", "notes.txt")
        self.assertEqual([finding.line_number for finding in findings], [2])

    def test_empty_text_has_no_findings(self):
        self.assertEqual(check_private_data.scan_text("", "empty.txt"), [])


class MaskingTest(unittest.TestCase):
    def test_masked_vin_keeps_only_the_manufacturer_prefix(self):
        masked_text = check_private_data.mask_matched_text(
            check_private_data.KIND_VIN_SHAPED, UNLISTED_VIN
        )
        self.assertEqual(masked_text, UNLISTED_VIN[:3] + "*" * 14)

    def test_masked_bluetooth_address_keeps_only_the_first_three_groups(self):
        masked_text = check_private_data.mask_matched_text(
            check_private_data.KIND_BLUETOOTH_ADDRESS_SHAPED, UNLISTED_BLUETOOTH_ADDRESS
        )
        self.assertEqual(masked_text, UNLISTED_BLUETOOTH_ADDRESS[:8] + ":**:**:**")


class AllowlistTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.allowlist_path = Path(self.temporary_directory.name) / "allowlist.txt"

    def test_entries_comments_and_blank_lines(self):
        self.allowlist_path.write_text(
            "# a comment\n\nSOMETEXT | the reason | with a second bar\n  OTHER|reason two  \n",
            encoding="utf-8",
        )
        self.assertEqual(
            check_private_data.load_allowlist(self.allowlist_path),
            {"SOMETEXT": "the reason | with a second bar", "OTHER": "reason two"},
        )

    def test_entry_without_a_reason_is_an_error(self):
        for malformed_line in ("SOMETEXT", "SOMETEXT |", "SOMETEXT |   ", "| only a reason"):
            with self.subTest(malformed_line=malformed_line):
                self.allowlist_path.write_text(malformed_line + "\n", encoding="utf-8")
                with self.assertRaises(check_private_data.CheckCouldNotRunError):
                    check_private_data.load_allowlist(self.allowlist_path)

    def test_missing_allowlist_file_is_an_error(self):
        with self.assertRaises(check_private_data.CheckCouldNotRunError):
            check_private_data.load_allowlist(self.allowlist_path)

    def test_real_allowlist_parses_and_holds_both_samples(self):
        reason_by_allowed_text = check_private_data.load_allowlist(REAL_ALLOWLIST_PATH)
        self.assertIn(SAMPLE_VIN, reason_by_allowed_text)
        self.assertIn(SAMPLE_BLUETOOTH_ADDRESS, reason_by_allowed_text)

    def test_unlisted_test_values_are_not_on_the_real_allowlist(self):
        reason_by_allowed_text = check_private_data.load_allowlist(REAL_ALLOWLIST_PATH)
        self.assertNotIn(UNLISTED_VIN, reason_by_allowed_text)
        self.assertNotIn(UNLISTED_BLUETOOTH_ADDRESS, reason_by_allowed_text)


class MainWithExplicitFilesTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.repository_root = Path(self.temporary_directory.name)

    def write_file(self, relative_path, content):
        file_path = self.repository_root / relative_path
        file_path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            file_path.write_bytes(content)
        else:
            file_path.write_text(content, encoding="utf-8")

    def run_check(self, *extra_arguments):
        return run_main(
            ["--repository-root", str(self.repository_root), "--allowlist", str(REAL_ALLOWLIST_PATH)]
            + list(extra_arguments)
        )

    def test_clean_file_exits_0(self):
        self.write_file("clean.txt", "nothing private here\n")
        exit_code, standard_output, _standard_error = self.run_check("clean.txt")
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_NO_FINDING)
        self.assertIn("1 files scanned, 0 skipped, 0 findings", standard_output)

    def test_vin_exits_1_and_reports_file_line_and_masked_match(self):
        self.write_file("docs/notes.md", f"line one\nvin {UNLISTED_VIN}\n")
        exit_code, standard_output, standard_error = self.run_check("docs/notes.md")
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_FINDING)
        self.assertIn("docs/notes.md:2: VIN-shaped string: " + UNLISTED_VIN[:3] + "*" * 14, standard_output)
        self.assertNotIn(UNLISTED_VIN, standard_output + standard_error)
        self.assertIn("FAILED", standard_error)

    def test_show_matches_prints_the_match_in_full(self):
        self.write_file("notes.md", f"{UNLISTED_BLUETOOTH_ADDRESS}\n")
        exit_code, standard_output, _standard_error = self.run_check("--show-matches", "notes.md")
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_FINDING)
        self.assertIn(
            "notes.md:1: Bluetooth-address-shaped string: " + UNLISTED_BLUETOOTH_ADDRESS,
            standard_output,
        )

    def test_allowlisted_samples_exit_0(self):
        self.write_file("samples.txt", f"{SAMPLE_VIN}\n{SAMPLE_BLUETOOTH_ADDRESS}\n")
        exit_code, _standard_output, _standard_error = self.run_check("samples.txt")
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_NO_FINDING)

    def test_allowlist_is_exact_so_a_variant_of_a_sample_is_reported(self):
        self.write_file("variant.txt", SAMPLE_BLUETOOTH_ADDRESS.lower() + "\n")
        exit_code, _standard_output, _standard_error = self.run_check("variant.txt")
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_FINDING)

    def test_vin_in_a_file_name_is_reported(self):
        relative_path = f"drive_{UNLISTED_VIN}_morning.csv"
        self.write_file(relative_path, "speed,rpm\n")
        exit_code, standard_output, _standard_error = self.run_check(relative_path)
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_FINDING)
        self.assertIn("(file name): VIN-shaped string", standard_output)

    def test_binary_file_is_skipped_and_listed(self):
        self.write_file("image.bin", b"\x00\x01" + UNLISTED_VIN.encode("ascii"))
        exit_code, standard_output, _standard_error = self.run_check("image.bin")
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_NO_FINDING)
        self.assertIn("skipped: image.bin (binary content)", standard_output)

    def test_bytes_that_are_not_utf8_do_not_hide_a_match(self):
        self.write_file("latin1.txt", b"caf\xe9 " + UNLISTED_VIN.encode("ascii") + b"\n")
        exit_code, _standard_output, _standard_error = self.run_check("latin1.txt")
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_FINDING)

    def test_missing_file_is_skipped_and_listed(self):
        exit_code, standard_output, _standard_error = self.run_check("deleted.txt")
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_NO_FINDING)
        self.assertIn("skipped: deleted.txt (not a regular file in the working tree)", standard_output)

    def test_malformed_allowlist_exits_2(self):
        self.write_file("allowlist.txt", "ENTRYWITHOUTREASON\n")
        self.write_file("clean.txt", "nothing private here\n")
        exit_code, _standard_output, standard_error = run_main(
            [
                "--repository-root",
                str(self.repository_root),
                "--allowlist",
                str(self.repository_root / "allowlist.txt"),
                "clean.txt",
            ]
        )
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_CHECK_COULD_NOT_RUN)
        self.assertIn("expected '<exact text> | <reason>'", standard_error)


@unittest.skipUnless(shutil.which("git"), "git is not installed on this machine")
class MainWithGitTrackedFilesTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.repository_root = Path(self.temporary_directory.name)
        subprocess.run(
            ["git", "init", "--quiet", str(self.repository_root)], check=True, capture_output=True
        )

    def track_file(self, relative_path, content):
        (self.repository_root / relative_path).write_text(content, encoding="utf-8")
        subprocess.run(
            ["git", "-C", str(self.repository_root), "add", relative_path],
            check=True,
            capture_output=True,
        )

    def run_check(self):
        return run_main(
            ["--repository-root", str(self.repository_root), "--allowlist", str(REAL_ALLOWLIST_PATH)]
        )

    def test_tracked_file_with_a_vin_exits_1(self):
        self.track_file("clean.txt", "nothing private here\n")
        self.track_file("recording.txt", f"{UNLISTED_VIN}\n")
        exit_code, standard_output, _standard_error = self.run_check()
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_FINDING)
        self.assertIn("recording.txt:1: VIN-shaped string", standard_output)
        self.assertIn("2 files scanned, 0 skipped, 1 findings", standard_output)

    def test_untracked_file_is_not_scanned(self):
        self.track_file("clean.txt", "nothing private here\n")
        (self.repository_root / "untracked.txt").write_text(f"{UNLISTED_VIN}\n", encoding="utf-8")
        exit_code, standard_output, _standard_error = self.run_check()
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_NO_FINDING)
        self.assertIn("1 files scanned, 0 skipped, 0 findings", standard_output)

    def test_folder_that_is_not_a_repository_exits_2(self):
        not_a_repository = tempfile.TemporaryDirectory()
        self.addCleanup(not_a_repository.cleanup)
        exit_code, _standard_output, standard_error = run_main(
            ["--repository-root", not_a_repository.name, "--allowlist", str(REAL_ALLOWLIST_PATH)]
        )
        self.assertEqual(exit_code, check_private_data.EXIT_CODE_CHECK_COULD_NOT_RUN)
        self.assertIn("git ls-files failed", standard_error)


if __name__ == "__main__":
    unittest.main()
