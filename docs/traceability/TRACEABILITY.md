# Traceability matrix

For every requirement: the design element that implements it, the ticket that builds it, and the test or measurement that verifies it. This file is updated in the same pull request that changes any of the three. A row whose test does not exist yet says "Planned" in the Status column and is reported as pending by `tools/check_traceability.py`, which CI runs on every pull request (since LHU-006). Any other row must point at a tagged test that exists, or at a measurement script or manual procedure file that exists; a tagged test whose row still says Planned fails the check, so the row is updated in the pull request that adds the test.

Design elements are named as in `docs/architecture/ARCHITECTURE.md`. Tests are named by the file that will carry the tag `// Verifies: REQ-nnn` (see `docs/test/TEST_STRATEGY.md` section 5). Every ticket is numbered since plan revision 6; the milestone each ticket belongs to is on the GitHub board.

## Matrix

| Requirement | Design element | Ticket | Test or measurement | Status |
|---|---|---|---|---|
| REQ-001 | `CommandAllowlist`, `Elm327Protocol` (single guarded send path); `CanFrameReader` (no send method) | LHU-010; LHU-028 | `tests/unit/obd/command_allowlist_test.cpp` (all 256 modes, Mode 04 named); `tests/unit/can/fake_can_frame_reader_test.cpp` (fails on write) | Planned |
| REQ-002 | `VehicleDataSource` interface, `SignalStoreFeeder`, `FakeSource`; source selection by configuration (LHU-012) | LHU-008; LHU-012; LHU-022; LHU-028; LHU-029 | `tests/integration/service_against_each_source_test.cpp` (parameterised by source harness: fake now, ELM327, CAN and replay as they land); `tests/unit/hardware/fake_source_test.cpp`; `tests/unit/service/signal_store_feeder_test.cpp`; CI step `tools/check_link_graph.py` on CMake's graphviz output | Tagged tests and the CI link check merged with LHU-008; further harnesses planned with LHU-012, LHU-028, LHU-029 |
| REQ-003 | `SignalSample`, `SignalStore`, `Clock` | LHU-006 | `tests/unit/service/signal_sample_test.cpp`, `tests/unit/service/signal_store_test.cpp`, `tests/unit/service/staleness_monitor_test.cpp`, `tests/unit/service/steady_clock_test.cpp` | Tagged tests merged with LHU-006 |
| REQ-004 | `ObdPidDecoder`; PID discovery in `Elm327ObdSource` | LHU-009; LHU-012 | `tests/unit/obd/obd_pid_decoder_test.cpp` (5 vectors per PID); `tests/integration/pid_discovery_test.cpp` (unsupported PID never requested over 100 cycles) | Planned |
| REQ-005 | `DbcDecoder` with `dbc/simulated_vehicle.dbc` | LHU-027 | `tests/unit/can/dbc_decoder_test.cpp` against a `cantools` oracle, 10,000 or more frames | Planned |
| REQ-006 | `StalenessMonitor`, `SignalDefinition` (per-signal timeout), `Clock` | LHU-006; LHU-013 and LHU-039 (styling) | `tests/unit/service/staleness_monitor_test.cpp`, `tests/unit/service/signal_store_test.cpp`; `tests/hmi/signal_tile_test.qml` (stale never drawn live) | Unit tests tagged and merged with LHU-006; the HMI styling test is planned (LHU-039) |
| REQ-007 | `ConnectionStateMachine` with the transition table of DN-007 | LHU-007; LHU-013 (display within 500 ms) | `tests/unit/service/connection_state_machine_test.cpp` (all 24 state and trigger pairs: 8 accepted, 16 rejected); `tests/hmi/connection_status_test.qml` | Unit test tagged and merged with LHU-007; the HMI timing test is planned (LHU-013) |
| REQ-008 | Reconnect loop and backoff in `Elm327ObdSource` | LHU-012 | `tests/scenarios/reconnect_after_emulator_restart_test.cpp` (20 trials) | Planned |
| REQ-009 | Whole path, source to rendered frame, including the D-Bus hop | LHU-032; LHU-022 (hop measured) | `tools/measure/measure_latency.py`; raw data in `docs/measurements/latency/` | Planned |
| REQ-010 | `Elm327Protocol`, `ObdPidDecoder`, `DbcDecoder` | LHU-010; LHU-009; LHU-027 | `tests/unit/obd/elm327_protocol_malformed_input_test.cpp` (named corpus plus 100,000 random strings under sanitizers); `tests/unit/can/dbc_decoder_malformed_frame_test.cpp` | Planned |
| REQ-011 | View models as the only QML binding target; `VehicleDataClient` as the only path from a view model to vehicle data | LHU-013; LHU-022 | CI include check in `ci.yml` | Planned |
| REQ-012 | QML home and vehicle-data screens, view models | LHU-013 (home); LHU-039 (vehicle data) | `tests/hmi/home_screen_test.qml`, `tests/hmi/vehicle_data_screen_test.qml` | Planned |
| REQ-013 | Service and hub startup, systemd units | LHU-032 | `tools/measure/measure_boot_time.py`; raw data in `docs/measurements/boot_time/` | Planned, target provisional |
| REQ-014 | Every process: service, hub, vehicle-data app, browser | LHU-032 | `tools/measure/measure_memory.py` (per process); raw data in `docs/measurements/memory/` | Planned, target provisional, to be re-set per process |
| REQ-015 | Recorder in `ByteTransport`, `ReplaySource`, scrub step | LHU-029 | `tests/integration/replay_reproduces_samples_test.cpp`; `tools/check_private_data.py` on scrubbed recordings | Planned |
| REQ-016 | `AppHub`, `AppRegistry`, `ProcessManager` | LHU-021 (after the LHU-020 spike) | `tests/integration/hub_process_cycles_test.cpp` (20 cycles, foreground within 1 s, same PID); `tests/hmi/hub_grid_test.qml` (8 targets at or above 10 mm) | Planned |
| REQ-017 | `VehicleDataService` (D-Bus adapter over the service layer), `VehicleDataClient` | LHU-022 | `tests/integration/dbus_two_clients_test.cpp`; `tests/integration/dbus_late_join_test.cpp`; hop time in `docs/measurements/latency/` | Planned |
| REQ-018 | URL entries in `AppRegistry`; browser launch configuration in `deploy/` | LHU-023 | `docs/test/MANUAL_WEB_APPS_PROCEDURE.md`; results in `docs/test/results/`; browser memory in `docs/measurements/memory/` | Planned |
| REQ-019 | Operating-system audio configuration in `deploy/`; connection log of `ConnectionStateMachine` | LHU-024 | `docs/test/MANUAL_ON_CAR_PROCEDURE.md` (audio section); results and logs in `docs/test/results/` | Planned |
| REQ-020 | `PowerStatusProvider` (real and fake), shutdown control in the hub | LHU-025 | `tests/unit/service/power_status_decoding_test.cpp`; `tests/hmi/power_flags_test.qml`; shutdown cycles in `docs/test/results/` | Planned |
| REQ-021 | `DtcDecoder`, `VehicleInfoDecoder`, `DiagnosticsModel`, diagnostics screen | LHU-030 | `tests/unit/obd/dtc_decoder_test.cpp` (0, 1, 2, 6 codes; P, C, B, U; multi-frame); `tests/unit/obd/vehicle_info_decoder_test.cpp`; `tests/hmi/diagnostics_screen_test.qml` | Planned |
| REQ-022 | `DerivedSignalEngine` and its derived-signal definitions | LHU-031 | `tests/unit/service/fuel_economy_test.cpp`, `tests/unit/service/trip_totals_test.cpp`; `tests/integration/replay_deterministic_totals_test.cpp` | Planned |

## Coverage summary

| | Count |
|---|---|
| Requirements | 22 |
| With a named design element | 22 |
| With a named ticket | 22 |
| With a named test or measurement | 22 |
| With a test that exists and is tagged | 4 (REQ-002, REQ-003, REQ-006, REQ-007) |

The last row is the number CI enforces through `tools/check_traceability.py`. It rises as tickets merge; a pull request that merges a test updates its row from Planned to the ticket that added it.

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
| LHU-015, LHU-016, LHU-018 | none; measurement tools and sessions (D-013) |
| LHU-017 | none; fixes the PID list of REQ-004 for this vehicle |
| LHU-019 | none; documentation (this revision) |
| LHU-020 | none; spike for REQ-016 |
| LHU-021 | REQ-016 |
| LHU-022 | REQ-017, REQ-002, REQ-011, REQ-009 (hop) |
| LHU-023 | REQ-018 |
| LHU-024 | REQ-019 |
| LHU-025 | REQ-020 |
| LHU-027 | REQ-005, REQ-010 |
| LHU-028 | REQ-001, REQ-002 |
| LHU-029 | REQ-015 |
| LHU-030 | REQ-021 |
| LHU-031 | REQ-022 |
| LHU-032 | REQ-009, REQ-013, REQ-014 |
| LHU-034 | none directly; the on-car procedure exercises REQ-004, REQ-006, REQ-007, REQ-008, REQ-019, REQ-020 |
| LHU-039 | REQ-012, REQ-006 |
| LHU-036, LHU-037, LHU-038 (roadmap) | requirements written when scheduled |
