#!/usr/bin/env python3
"""Scrub the vehicle identification number from a recording (REQ-015, DN-029, D-023).

A recording (`local_recordings/*.rec`, written by RecordingByteTransport) stores the adapter's
bytes as hexadecimal. The VIN can hide in them in two forms:

  * plain text: 17 VIN-shaped characters in the adapter's text;
  * Mode 09 encoding: the reply to 0902 carries the VIN as hexadecimal byte pairs
    ("49 02 01 31 4D 38 ..."), with or without spaces, possibly over several lines that start
    with a frame counter ("0:", "1:").

All read events are joined into one stream (a reply can be split across reads), both forms
are searched with the shape tools/check_private_data.py uses, and every VIN character is replaced
by "0" (byte 0x30) in the form it was found, so lengths and line structure stay the same and
the scrubbed recording still replays. The result is checked again in all forms; if anything
VIN-shaped is left, nothing is written.

    python3 tools/scrub_recording.py local_recordings/drive.rec --output drive.scrubbed.rec

Exit 0 written, 1 a VIN remained (nothing written), 2 could not run.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import List, Optional, Sequence, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_private_data import KIND_VIN_SHAPED, find_shaped_strings  # noqa: E402

EXIT_CODE_WRITTEN = 0
EXIT_CODE_VIN_REMAINED = 1
EXIT_CODE_COULD_NOT_RUN = 2

READ_EVENT = "READ"
FRAME_COUNTER_PATTERN = re.compile(r"^\s*[0-9A-Fa-f]{1,2}:")
HEX_PAIR_PATTERN = re.compile(r"[0-9A-Fa-f]{2}")
VIN_CHARACTER_REPLACEMENT = "0"
VIN_HEX_REPLACEMENT = "30"


class RecordingLine:
    def __init__(self, text: str):
        self.text = text
        parts = text.split()
        self.is_read = len(parts) >= 2 and parts[1] == READ_EVENT and not text.startswith("#")
        self.prefix = " ".join(parts[:2]) if self.is_read else text
        self.payload = bytes.fromhex(parts[2]) if self.is_read and len(parts) > 2 else b""

    def rendered(self, payload: bytes) -> str:
        if not self.is_read:
            return self.text
        return f"{self.prefix} {payload.hex()}"


def vin_spans_in_plain_text(text: str) -> List[Tuple[int, int]]:
    spans = []
    for kind, matched in find_shaped_strings(text):
        if kind != KIND_VIN_SHAPED:
            continue
        start = text.find(matched)
        while start >= 0:
            spans.append((start, start + len(matched)))
            start = text.find(matched, start + 1)
    return spans


def hex_pairs_by_line(text: str) -> List[List[Tuple[int, int]]]:
    """For each line of the adapter's text, the (position, byte) of every hex pair after an optional frame counter."""
    lines = []
    position = 0
    for line in text.splitlines(keepends=True):
        body_start = 0
        counter = FRAME_COUNTER_PATTERN.match(line)
        if counter:
            body_start = counter.end()
        pairs = []
        for match in HEX_PAIR_PATTERN.finditer(line, body_start):
            pairs.append((position + match.start(), int(match.group(0), 16)))
        lines.append(pairs)
        position += len(line)
    return lines


def vin_positions_in_hex_encoding(text: str) -> List[int]:
    """Text positions of the hex pairs that encode VIN characters."""
    pairs = [pair for line in hex_pairs_by_line(text) for pair in line]
    decoded = "".join(chr(value) if 32 <= value < 127 else "\x00" for _, value in pairs)
    positions = []
    for start, end in vin_spans_in_plain_text(decoded):
        positions.extend(pairs[index][0] for index in range(start, end))
    return positions


def scrub_text(text: str) -> Tuple[str, int]:
    characters = list(text)
    replaced = 0
    for start, end in vin_spans_in_plain_text(text):
        for index in range(start, end):
            characters[index] = VIN_CHARACTER_REPLACEMENT
        replaced += 1
    for position in vin_positions_in_hex_encoding("".join(characters)):
        characters[position] = VIN_HEX_REPLACEMENT[0]
        characters[position + 1] = VIN_HEX_REPLACEMENT[1]
        replaced += 1
    return "".join(characters), replaced


def remaining_vins(text: str) -> int:
    return len(vin_spans_in_plain_text(text)) + len(vin_positions_in_hex_encoding(text))


def scrub_recording(recording_text: str) -> Tuple[str, int, int]:
    """Returns (scrubbed recording, VIN characters or encodings replaced, VINs remaining)."""
    lines = [RecordingLine(line) for line in recording_text.splitlines()]
    stream = b"".join(line.payload for line in lines if line.is_read)
    text = stream.decode("latin-1")
    scrubbed_text, replaced = scrub_text(text)
    scrubbed_stream = scrubbed_text.encode("latin-1")
    output_lines = []
    offset = 0
    for line in lines:
        if line.is_read:
            length = len(line.payload)
            output_lines.append(line.rendered(scrubbed_stream[offset : offset + length]))
            offset += length
        else:
            output_lines.append(line.text)
    output = "\n".join(output_lines) + "\n"
    remaining = remaining_vins(scrubbed_text) + len(
        [kind for kind, _ in find_shaped_strings(output) if kind == KIND_VIN_SHAPED]
    )
    return output, replaced, remaining


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Scrub the VIN from a recording.")
    parser.add_argument("recording", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args(argument_list)
    try:
        text = arguments.recording.read_text(encoding="ascii")
    except (OSError, UnicodeDecodeError) as error:
        print(f"scrub_recording: error: {error}", file=sys.stderr)
        return EXIT_CODE_COULD_NOT_RUN
    output, replaced, remaining = scrub_recording(text)
    if remaining:
        print(f"scrub_recording: {remaining} VIN-shaped strings remain; nothing written", file=sys.stderr)
        return EXIT_CODE_VIN_REMAINED
    arguments.output.write_text(output, encoding="ascii")
    print(f"scrub_recording: {replaced} VIN occurrences replaced; written to {arguments.output}")
    return EXIT_CODE_WRITTEN


if __name__ == "__main__":
    sys.exit(main())
