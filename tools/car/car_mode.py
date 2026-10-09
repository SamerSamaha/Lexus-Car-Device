#!/usr/bin/env python3
"""Switches the vehicle-data service between car, demo, replay and emulator mode (LHU-044).

    lexus-mode show
    lexus-mode car                 the adapter in the car
    lexus-mode demo                moving demonstration values, no car and no adapter
    lexus-mode replay [FILE|last]  a recorded session; "last" is the newest car session
    lexus-mode emulator            the ELM327 emulator on the Pi itself

The mode is written into car.conf, then the service is restarted, which starts a new session
folder. The hub runs this as a short app, so a tap on "Demo mode" or "Car mode" switches.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import Callable, List, Optional, Sequence

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

import car_config  # noqa: E402

SERVICE_UNIT = "lexus-vehicle-data-service.service"
EMULATOR_UNIT = "lexus-elm327-emulator.service"

Runner = Callable[[List[str]], int]


def run_command(command: List[str]) -> int:
    return subprocess.run(command, check=False).returncode


def switch(config: Path, mode: str, replay_file: Optional[str], runner: Runner) -> int:
    updates = {"mode.kind": mode}
    if mode == "replay":
        updates["mode.replay_file"] = replay_file or "last"
    car_config.write_values(config, updates)
    emulator_action = "start" if mode == "emulator" else "stop"
    status = runner(["systemctl", "--user", emulator_action, EMULATOR_UNIT])
    if mode == "emulator" and status != 0:
        print("lexus-mode: the emulator unit did not start", file=sys.stderr)
        return status
    status = runner(["systemctl", "--user", "restart", SERVICE_UNIT])
    print(f"lexus-mode: {mode}" + (f" ({updates['mode.replay_file']})" if mode == "replay" else "")
          + ("" if status == 0 else f"; restarting the service failed ({status})"))
    return status


def main(argument_list: Optional[Sequence[str]] = None, runner: Runner = run_command) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("mode", choices=("show",) + car_config.MODES)
    parser.add_argument("replay_file", nargs="?", default=None)
    parser.add_argument("--config", type=Path, default=car_config.DEFAULT_PATH)
    arguments = parser.parse_args(argument_list)
    if arguments.replay_file is not None and arguments.mode != "replay":
        parser.error("a file is given only with replay")
    if arguments.mode == "show":
        values = car_config.read(arguments.config)
        mode = car_config.mode_of(values)
        extra = f" ({values.get('mode.replay_file', 'last') or 'last'})" if mode == "replay" else ""
        print(f"mode {mode}{extra}")
        return 0
    return switch(arguments.config, arguments.mode, arguments.replay_file, runner)


if __name__ == "__main__":
    sys.exit(main())
