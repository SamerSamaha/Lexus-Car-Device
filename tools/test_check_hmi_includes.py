#!/usr/bin/env python3
"""Tests for check_hmi_includes.py."""

import contextlib
import io
import tempfile
import unittest
from pathlib import Path

import check_hmi_includes


def run_main(repository_root):
    standard_output = io.StringIO()
    standard_error = io.StringIO()
    with contextlib.redirect_stdout(standard_output), contextlib.redirect_stderr(standard_error):
        exit_code = check_hmi_includes.main(["--repository-root", str(repository_root)])
    return exit_code, standard_output.getvalue(), standard_error.getvalue()


class CheckHmiIncludesTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.root = Path(self.temporary_directory.name)
        (self.root / "src" / "hmi" / "viewmodels").mkdir(parents=True)
        (self.root / "src" / "hmi" / "qml").mkdir(parents=True)

    def write(self, relative_path, text):
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def test_value_type_includes_and_qt_imports_pass(self):
        self.write("src/hmi/viewmodels/a.h", '#include "lexus_head_unit/service/signal_sample.h"\n#include <QObject>\n')
        self.write("src/hmi/qml/A.qml", "import QtQuick\nimport QtQuick.Window\nimport LexusHeadUnit\nItem {}\n")
        exit_code, output, _ = run_main(self.root)
        self.assertEqual(exit_code, check_hmi_includes.EXIT_CODE_NO_FINDING, output)

    def test_hardware_include_fails(self):
        self.write("src/hmi/viewmodels/a.cpp", '#include "lexus_head_unit/hardware/elm327_obd_source.h"\n')
        exit_code, output, _ = run_main(self.root)
        self.assertEqual(exit_code, check_hmi_includes.EXIT_CODE_FINDING)
        self.assertIn("hardware header", output)

    def test_store_include_fails(self):
        self.write("src/hmi/viewmodels/a.cpp", '#include "lexus_head_unit/service/signal_store.h"\n')
        exit_code, output, _ = run_main(self.root)
        self.assertEqual(exit_code, check_hmi_includes.EXIT_CODE_FINDING)
        self.assertIn("not a value type", output)

    def test_stray_qml_import_fails(self):
        self.write("src/hmi/qml/A.qml", 'import QtQuick\nimport "../../hardware"\nItem {}\n')
        exit_code, output, _ = run_main(self.root)
        self.assertEqual(exit_code, check_hmi_includes.EXIT_CODE_FINDING)
        self.assertIn("imports outside", output)

    def test_missing_hmi_folder_cannot_run(self):
        with tempfile.TemporaryDirectory() as empty:
            exit_code, _, error = run_main(Path(empty))
        self.assertEqual(exit_code, check_hmi_includes.EXIT_CODE_CHECK_COULD_NOT_RUN)
        self.assertIn("not a directory", error)


if __name__ == "__main__":
    unittest.main()
