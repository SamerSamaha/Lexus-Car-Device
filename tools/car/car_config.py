"""Reads and edits the car-mode file of one Pi, ~/.config/lexus-head-unit/car.conf (LHU-044).

The file has the format of deploy/head_unit.conf: "[section]" lines, "key = value" lines and
"#" comments. Keys are read as "section.key". It is created from deploy/car/car.conf.example by
the install script and is never committed, because it holds the adapter's Bluetooth address
(privacy rule). Edits keep every comment and line order; a missing key is added at the end of
its section, a missing section at the end of the file.
"""

import os
import re
import tempfile
from pathlib import Path
from typing import Dict, List, Mapping, Optional

DEFAULT_PATH = Path.home() / ".config" / "lexus-head-unit" / "car.conf"
MODES = ("car", "demo", "replay", "emulator")
ADDRESS_PATTERN = re.compile(r"^[0-9A-F]{2}(:[0-9A-F]{2}){5}$")

_SECTION = re.compile(r"^\s*\[([^\]]+)\]\s*$")
_ENTRY = re.compile(r"^(\s*)([^#;=\s][^=]*?)\s*=\s*(.*?)\s*$")


def parse(text: str) -> Dict[str, str]:
    values: Dict[str, str] = {}
    section = ""
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped[0] in "#;":
            continue
        section_match = _SECTION.match(line)
        if section_match:
            section = section_match.group(1).strip()
            continue
        entry = _ENTRY.match(line)
        if entry:
            key = entry.group(2).strip()
            values[f"{section}.{key}" if section else key] = entry.group(3)
    return values


def read(path: Path = DEFAULT_PATH) -> Dict[str, str]:
    try:
        return parse(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return {}


def with_values(text: str, updates: Mapping[str, str]) -> str:
    """The text with each "section.key" set to its value, everything else unchanged."""
    lines: List[str] = text.splitlines()
    for full_key, value in updates.items():
        section, _, key = full_key.rpartition(".")
        lines = _set_one(lines, section, key, value)
    return "\n".join(lines) + "\n"


def _set_one(lines: List[str], section: str, key: str, value: str) -> List[str]:
    lines = list(lines)
    current = ""
    section_found = section == ""
    insert_after = -1
    for index, line in enumerate(lines):
        section_match = _SECTION.match(line)
        if section_match:
            current = section_match.group(1).strip()
            if current == section:
                section_found = True
                insert_after = index
            continue
        if current != section:
            continue
        entry = _ENTRY.match(line)
        if entry and entry.group(2).strip() == key:
            lines[index] = f"{entry.group(1)}{key} = {value}"
            return lines
        if line.strip():
            insert_after = index
    if not section_found:
        if lines and lines[-1].strip():
            lines.append("")
        lines.extend([f"[{section}]", f"{key} = {value}"])
        return lines
    lines.insert(insert_after + 1, f"{key} = {value}")
    return lines


def write_values(path: Path, updates: Mapping[str, str]) -> None:
    """Sets the values and replaces the file atomically, keeping it private to the user."""
    try:
        text = path.read_text(encoding="utf-8")
    except FileNotFoundError:
        text = ""
    new_text = with_values(text, updates)
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(dir=str(path.parent), prefix=".car.conf.")
    try:
        with os.fdopen(handle, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(new_text)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temporary, 0o600)
        os.replace(temporary, path)
    except BaseException:
        if os.path.exists(temporary):
            os.unlink(temporary)
        raise


def normalised_address(text: str) -> Optional[str]:
    candidate = text.strip().upper().replace("-", ":")
    return candidate if ADDRESS_PATTERN.match(candidate) else None


def masked_address(address: str) -> str:
    """Only the last two bytes, for output that may end up in a pasted log."""
    parts = address.split(":")
    return ":".join(["XX"] * (len(parts) - 2) + parts[-2:]) if len(parts) == 6 else "XX"


def mode_of(values: Mapping[str, str]) -> str:
    mode = values.get("mode.kind", "car").strip().lower()
    return mode if mode in MODES else "car"


def data_directory(values: Mapping[str, str]) -> Path:
    return Path(os.path.expanduser(values.get("session.data_dir", "~/lexus-data") or "~/lexus-data"))
