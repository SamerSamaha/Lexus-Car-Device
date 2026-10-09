#!/usr/bin/env python3
"""Fail if a service or HMI target links, directly or transitively, to a concrete data source.

Input: the graphviz file CMake writes with `cmake --preset <name> --graphviz=<file>`. Nodes are
targets; edges are link dependencies. A concrete source is any target whose name ends in
"_source". The guarded targets are the service library, the D-Bus service and client library,
and every target whose name starts with "lexus_head_unit_hmi" or "lexus_head_unit_hub" (the hub
never talks to the vehicle). The applications, the vehicle-data service executable, the source
wiring library and the test executables are free to link sources, because they are the wiring
(REQ-002).

Exit 0 clean, 1 finding, 2 could not run.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Set

EXIT_CODE_NO_FINDING = 0
EXIT_CODE_FINDING = 1
EXIT_CODE_CHECK_COULD_NOT_RUN = 2

GUARDED_EXACT_TARGETS = ("lexus_head_unit_service", "lexus_head_unit_service_dbus")
GUARDED_PREFIXES = ("lexus_head_unit_hmi", "lexus_head_unit_hub")
CONCRETE_SOURCE_SUFFIX = "_source"

NODE_PATTERN = re.compile(r'^\s*"(?P<node>[^"]+)"\s*\[\s*label\s*=\s*"(?P<label>[^"]+)"')
EDGE_PATTERN = re.compile(r'^\s*"(?P<from>[^"]+)"\s*->\s*"(?P<to>[^"]+)"')


class CheckCouldNotRunError(Exception):
    """The graph file is missing or holds no targets."""


def parse_graph(graph_text: str) -> Dict[str, Set[str]]:
    """Return a map from target name to the set of target names it links to."""
    label_by_node: Dict[str, str] = {}
    edges: List[tuple] = []
    for line in graph_text.splitlines():
        node_match = NODE_PATTERN.match(line)
        if node_match:
            label_by_node[node_match.group("node")] = node_match.group("label")
            continue
        edge_match = EDGE_PATTERN.match(line)
        if edge_match:
            edges.append((edge_match.group("from"), edge_match.group("to")))
    if not label_by_node:
        raise CheckCouldNotRunError("no target nodes found in the graph")
    dependencies: Dict[str, Set[str]] = {label: set() for label in label_by_node.values()}
    for from_node, to_node in edges:
        if from_node in label_by_node and to_node in label_by_node:
            dependencies[label_by_node[from_node]].add(label_by_node[to_node])
    return dependencies


def reachable_from(target: str, dependencies: Dict[str, Set[str]]) -> Set[str]:
    """Every target reachable from the given one along link edges."""
    seen: Set[str] = set()
    pending = list(dependencies.get(target, ()))
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        pending.extend(dependencies.get(current, ()))
    return seen


def is_guarded(target: str) -> bool:
    return target in GUARDED_EXACT_TARGETS or target.startswith(GUARDED_PREFIXES)


def is_concrete_source(target: str) -> bool:
    return target.endswith(CONCRETE_SOURCE_SUFFIX)


def find_violations(dependencies: Dict[str, Set[str]]) -> List[str]:
    violations: List[str] = []
    for target in sorted(dependencies):
        if not is_guarded(target):
            continue
        reached_sources = sorted(filter(is_concrete_source, reachable_from(target, dependencies)))
        for source in reached_sources:
            violations.append(f"{target} links to the concrete source {source}")
    return violations


def parse_arguments(argument_list: Optional[Sequence[str]]) -> argparse.Namespace:
    argument_parser = argparse.ArgumentParser(
        description="Fail if a service or HMI target links to a concrete data source."
    )
    argument_parser.add_argument("graph_file", type=Path, help="graphviz file written by CMake")
    return argument_parser.parse_args(argument_list)


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    arguments = parse_arguments(argument_list)
    try:
        dependencies = parse_graph(arguments.graph_file.read_text(encoding="utf-8"))
    except OSError as read_error:
        print(f"check_link_graph: error: cannot read {arguments.graph_file}: {read_error}", file=sys.stderr)
        return EXIT_CODE_CHECK_COULD_NOT_RUN
    except CheckCouldNotRunError as check_error:
        print(f"check_link_graph: error: {check_error}", file=sys.stderr)
        return EXIT_CODE_CHECK_COULD_NOT_RUN

    guarded = sorted(filter(is_guarded, dependencies))
    sources = sorted(filter(is_concrete_source, dependencies))
    for target in guarded:
        reached = sorted(reachable_from(target, dependencies))
        print(f"{target} -> {', '.join(reached) if reached else '(nothing)'}")
    violations = find_violations(dependencies)
    for violation in violations:
        print(violation)
    print(
        f"check_link_graph: {len(dependencies)} targets, {len(guarded)} guarded, "
        f"{len(sources)} concrete sources, {len(violations)} violations"
    )
    if violations:
        print("check_link_graph: FAILED.", file=sys.stderr)
        return EXIT_CODE_FINDING
    return EXIT_CODE_NO_FINDING


if __name__ == "__main__":
    sys.exit(main())
