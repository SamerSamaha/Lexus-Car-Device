#!/usr/bin/env python3
"""Tests for check_link_graph.py."""

import contextlib
import io
import tempfile
import unittest
from pathlib import Path

import check_link_graph

CLEAN_GRAPH = """digraph "LexusHeadUnit" {
    "node0" [ label = "GTest::gtest_main", shape = octagon ];
    "node1" [ label = "lexus_head_unit_build_settings", shape = pentagon ];
    "node2" [ label = "lexus_head_unit_service", shape = octagon ];
    "node3" [ label = "lexus_head_unit_fake_source", shape = octagon ];
    "node4" [ label = "lexus_head_unit_unit_tests", shape = egg ];
    "node2" -> "node1"  // lexus_head_unit_service -> lexus_head_unit_build_settings
    "node3" -> "node2"  // lexus_head_unit_fake_source -> lexus_head_unit_service
    "node4" -> "node3"  // lexus_head_unit_unit_tests -> lexus_head_unit_fake_source
    "node4" -> "node0"
}
"""

VIOLATING_GRAPH = CLEAN_GRAPH.replace(
    '"node4" -> "node0"',
    '"node4" -> "node0"\n    "node5" [ label = "lexus_head_unit_hmi_viewmodels", shape = octagon ];\n'
    '    "node6" [ label = "lexus_head_unit_helper", shape = octagon ];\n'
    '    "node5" -> "node6"\n    "node6" -> "node3"',
)


def run_main(graph_text):
    with tempfile.TemporaryDirectory() as directory:
        graph_path = Path(directory) / "graph.dot"
        graph_path.write_text(graph_text, encoding="utf-8")
        standard_output = io.StringIO()
        standard_error = io.StringIO()
        with contextlib.redirect_stdout(standard_output), contextlib.redirect_stderr(standard_error):
            exit_code = check_link_graph.main([str(graph_path)])
    return exit_code, standard_output.getvalue(), standard_error.getvalue()


class CheckLinkGraphTest(unittest.TestCase):
    def test_parses_nodes_and_edges_by_label(self):
        dependencies = check_link_graph.parse_graph(CLEAN_GRAPH)
        self.assertEqual(dependencies["lexus_head_unit_fake_source"], {"lexus_head_unit_service"})
        self.assertEqual(
            dependencies["lexus_head_unit_unit_tests"],
            {"lexus_head_unit_fake_source", "GTest::gtest_main"},
        )

    def test_service_without_a_source_dependency_passes(self):
        exit_code, output, _ = run_main(CLEAN_GRAPH)
        self.assertEqual(exit_code, check_link_graph.EXIT_CODE_NO_FINDING, output)
        self.assertIn("0 violations", output)

    def test_test_executables_may_link_sources(self):
        exit_code, _, _ = run_main(CLEAN_GRAPH)
        self.assertEqual(exit_code, check_link_graph.EXIT_CODE_NO_FINDING)

    def test_transitive_link_from_an_hmi_target_to_a_source_fails(self):
        exit_code, output, _ = run_main(VIOLATING_GRAPH)
        self.assertEqual(exit_code, check_link_graph.EXIT_CODE_FINDING)
        self.assertIn(
            "lexus_head_unit_hmi_viewmodels links to the concrete source lexus_head_unit_fake_source",
            output,
        )

    def test_service_linking_a_source_fails(self):
        graph = CLEAN_GRAPH.replace('"node4" -> "node0"', '"node4" -> "node0"\n    "node2" -> "node3"')
        exit_code, output, _ = run_main(graph)
        self.assertEqual(exit_code, check_link_graph.EXIT_CODE_FINDING)
        self.assertIn("lexus_head_unit_service links to the concrete source", output)

    def test_empty_graph_cannot_run(self):
        exit_code, _, error = run_main("digraph x {}\n")
        self.assertEqual(exit_code, check_link_graph.EXIT_CODE_CHECK_COULD_NOT_RUN)
        self.assertIn("no target nodes", error)

    def test_missing_file_cannot_run(self):
        standard_error = io.StringIO()
        with contextlib.redirect_stderr(standard_error):
            exit_code = check_link_graph.main(["/nonexistent/graph.dot"])
        self.assertEqual(exit_code, check_link_graph.EXIT_CODE_CHECK_COULD_NOT_RUN)


if __name__ == "__main__":
    unittest.main()
