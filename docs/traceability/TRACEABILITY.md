# Traceability matrix

For every requirement: the design element that implements it, the ticket that builds it, and the test or measurement that verifies it. This file is updated in the same pull request that changes any of the three. A row whose test does not exist yet says so in the Status column; CI will fail the pull request once `tools/check_traceability.py` exists (LHU-006) and a requirement has no tagged test.

Design elements are named as in `docs/architecture/ARCHITECTURE.md`. Tests are named by the file that will carry the tag `// Verifies: REQ-nnn` (see `docs/test/TEST_STRATEGY.md` section 5). Sprint 2 tickets that are not yet numbered are written as "S2".

## Matrix

| Requirement | Design element | Ticket | Test or measurement | Status on 2026-10-03 |
|---|---|---|---|---|
| REQ-001 | `CommandAllowlist`, `Elm327Protocol` (single guarded send path); `CanFrameReader` (no send method) | LHU-010; S2 (CAN source) | `tests/unit/obd/command_allowlist_test.cpp` (all 256 modes, Mode 04 named); `tests/unit/can/fake_can_frame_reader_test.cpp` (fails on write) | Planned |
| REQ-002 | `VehicleDataSource` interface, `FakeSource`; source selection in `app` | LHU-008; LHU-012 | `tests/integration/service_against_each_source_test.cpp`; CI link-graph check in `ci.yml` | Planned |
| REQ-003 | `SignalSample`, `SignalStore` | LHU-006 | `tests/unit/service/signal_sample_test.cpp`, `tests/unit/service/signal_store_test.cpp` | Planned |
| REQ-004 | `ObdPidDecoder`; PID discovery in `Elm327ObdSource` | LHU-009; LHU-012 | `tests/unit/obd/obd_pid_decoder_test.cpp` (5 vectors per PID); `tests/integration/pid_discovery_test.cpp` (unsupported PID never requested over 100 cycles) | Planned |
| REQ-005 | `DbcDecoder` with `dbc/simulated_vehicle.dbc` | S2 (DBC decoder) | `tests/unit/can/dbc_decoder_test.cpp` against a `cantools` oracle, 10,000 or more frames | Planned |
| REQ-006 | `StalenessMonitor`, `SignalDefinition` (per-signal timeout), `Clock` | LHU-006; LHU-013 (styling) | `tests/unit/service/staleness_monitor_test.cpp`; `tests/hmi/signal_tile_test.qml` (stale never drawn live) | Planned |
| REQ-007 | `ConnectionStateMachine` with the transition table of DN-007 | LHU-007; LHU-013 (display within 500 ms) | `tests/unit/service/connection_state_machine_test.cpp` (every legal and illegal transition); `tests/hmi/connection_status_test.qml` | Planned |
| REQ-008 | Reconnect loop and backoff in `Elm327ObdSource` | LHU-012 | `tests/scenarios/reconnect_after_emulator_restart_test.cpp` (20 trials) | Planned |
| REQ-009 | Whole path, source to rendered frame | Sprint 3 (measurement) | `tools/measure/measure_latency.py`; raw data in `docs/measurements/latency/` | Planned |
| REQ-010 | `Elm327Protocol`, `ObdPidDecoder`, `DbcDecoder` | LHU-010; LHU-009; S2 (DBC decoder) | `tests/unit/obd/elm327_protocol_malformed_input_test.cpp` (named corpus plus 100,000 random strings under sanitizers); `tests/unit/can/dbc_decoder_malformed_frame_test.cpp` | Planned |
| REQ-011 | View models as the only QML binding target | LHU-013 | CI include check in `ci.yml` | Planned |
| REQ-012 | QML home and vehicle data screens, view models | LHU-013 (home, stretch); sprint 3 (vehicle data) | `tests/hmi/home_screen_test.qml`, `tests/hmi/vehicle_data_screen_test.qml` | Planned |
| REQ-013 | `app` startup, systemd unit | Sprint 3 (measurement) | `tools/measure/measure_boot_time.py`; raw data in `docs/measurements/boot_time/` | Planned, target provisional |
| REQ-014 | Whole application | Sprint 3 (measurement) | `tools/measure/measure_memory.py`; raw data in `docs/measurements/memory/` | Planned, target provisional |
| REQ-015 | Recorder in `ByteTransport`, `ReplaySource`, scrub step | S2 (record and replay) | `tests/integration/replay_reproduces_samples_test.cpp`; `tools/check_private_data.py` on scrubbed recordings | Planned |

## Coverage summary

| | Count |
|---|---|
| Requirements | 15 |
| With a named design element | 15 |
| With a named ticket | 15 |
| With a named test or measurement | 15 |
| With a test that exists and is tagged | 0 |

The last row is the number CI will enforce. It rises as tickets merge; a pull request that merges a test updates its row from Planned to the commit that added it.

## Reverse view: ticket to requirements

| Ticket | Requirements |
|---|---|
| LHU-006 | REQ-003, REQ-006 |
| LHU-007 | REQ-007 |
| LHU-008 | REQ-002 |
| LHU-009 | REQ-004, REQ-010 |
| LHU-010 | REQ-001, REQ-010 |
| LHU-011 | none directly; the emulator is the test fixture for REQ-004, REQ-008, REQ-010 |
| LHU-012 | REQ-002, REQ-004, REQ-008 |
| LHU-013 | REQ-006, REQ-007, REQ-011, REQ-012 |
| LHU-015 | none; it is a measurement tool (D-013) |
| S2, DBC decoder and CAN source | REQ-001, REQ-005, REQ-010 |
| S2, record and replay | REQ-015 |
| Sprint 3 measurements | REQ-009, REQ-013, REQ-014 |
