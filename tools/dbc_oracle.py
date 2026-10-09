#!/usr/bin/env python3
"""Independent oracle for the DBC decoder (REQ-005, DN-027).

Loads a DBC file with the cantools library, generates frames from a fixed seed (a random
message of the database, random bytes of that message's length), decodes each frame with
cantools, and writes one CSV line per decoded signal:

    frame,identifier,extended,data,signal,value

identifier is decimal, data is the frame's bytes in hexadecimal, value is Python's repr() of
the scaled float, so no digit is lost. The C++ test decodes the same frames with DbcDecoder and
compares every value. Runs only in the test-only virtual environment (tools/setup_test_venv.sh);
cantools is never a dependency of the product.

Exit 0 written, 2 could not run.
"""

from __future__ import annotations

import argparse
import csv
import random
import sys
from pathlib import Path
from typing import Optional, Sequence

EXIT_CODE_WRITTEN = 0
EXIT_CODE_COULD_NOT_RUN = 2


def parse_arguments(argument_list: Optional[Sequence[str]]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Decode random frames with cantools.")
    parser.add_argument("--dbc", type=Path, required=True, help="DBC file")
    parser.add_argument("--frames", type=int, default=10000, help="number of frames (default 10000)")
    parser.add_argument("--seed", type=int, default=20261009, help="random seed")
    parser.add_argument("--output", type=Path, required=True, help="CSV file to write")
    return parser.parse_args(argument_list)


def write_oracle(dbc_path: Path, frame_count: int, seed: int, output_path: Path) -> int:
    import cantools  # imported here so that --help works without the virtual environment

    database = cantools.database.load_file(str(dbc_path))
    messages = sorted(database.messages, key=lambda message: message.frame_id)
    generator = random.Random(seed)
    line_count = 0
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="", encoding="ascii") as output_file:
        writer = csv.writer(output_file, lineterminator="\n")
        writer.writerow(["frame", "identifier", "extended", "data", "signal", "value"])
        for frame_number in range(frame_count):
            message = generator.choice(messages)
            data = bytes(generator.getrandbits(8) for _ in range(message.length))
            decoded = message.decode(data, decode_choices=False, scaling=True)
            for signal in message.signals:
                writer.writerow(
                    [
                        frame_number,
                        message.frame_id,
                        1 if message.is_extended_frame else 0,
                        data.hex(),
                        signal.name,
                        repr(float(decoded[signal.name])),
                    ]
                )
                line_count += 1
    print(
        f"dbc_oracle: cantools {cantools.__version__}, seed {seed}, {frame_count} frames, "
        f"{line_count} signal values, written to {output_path}"
    )
    return line_count


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    arguments = parse_arguments(argument_list)
    try:
        write_oracle(arguments.dbc, arguments.frames, arguments.seed, arguments.output)
    except ImportError as error:
        print(
            f"dbc_oracle: error: {error}. Run tools/setup_test_venv.sh and use its python3.",
            file=sys.stderr,
        )
        return EXIT_CODE_COULD_NOT_RUN
    except OSError as error:
        print(f"dbc_oracle: error: {error}", file=sys.stderr)
        return EXIT_CODE_COULD_NOT_RUN
    return EXIT_CODE_WRITTEN


if __name__ == "__main__":
    sys.exit(main())
