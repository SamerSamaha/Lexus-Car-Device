# ELM327 emulator

Behaves like an ELM327 adapter on a running engine, on a pseudo-terminal, with fault injection over a control socket. Design: `docs/design/DN-011-elm327-emulator.md`. Standard library only; Linux.

Start it:

```sh
python3 tools/elm327_emulator/elm327_emulator.py --link /tmp/obd --control /tmp/obd.control --dtc P0133 P0420
```

The first line printed is the device; `/tmp/obd` always points at the current one, so a restarted emulator is found at the same path. Open `/tmp/obd` as a serial device.

Inject faults (one line per command, `OK`, `ERR reason`, or JSON for `stats`):

```sh
printf 'stale 0D\n' | socat - UNIX-CONNECT:/tmp/obd.control     # or any Unix-socket client
printf 'stats\n'    | socat - UNIX-CONNECT:/tmp/obd.control
```

| Command | Effect |
|---|---|
| `silence SECONDS` | no replies at all for that long |
| `stale PID` / `unstale PID` | that Mode 01 PID answers `NO DATA` |
| `corrupt N` | the next N data replies have one byte garbled |
| `delay MILLISECONDS` | every reply is delayed |
| `ignition off` / `ignition on` | OBD requests answer `UNABLE TO CONNECT` |
| `stats` | request counts, including requests outside the allowlist |
| `exit` | close the device (the serial side sees end-of-file) and quit |

Profiles (`--profile`): `standard` (the 8 PIDs of REQ-004 and the usual neighbours), `no-fuel-level` (PID 0x2F absent, for the discovery test), `minimal` (RPM and speed only). The vehicle identification number returned by Mode 09 is the documented hypothetical value from `tools/private_data_allowlist.txt` unless `--vin` is given.

Unit tests: `python3 -m unittest discover --start-directory tools --pattern "test_elm327_emulator.py"`.
