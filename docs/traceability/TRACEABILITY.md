# Traceability matrix

For every requirement: the design element that implements it, the ticket that builds it, and the test or measurement that verifies it. This file is updated in the same pull request that changes any of the three. A row whose test does not exist yet says "Planned" in the Status column and is reported as pending by `tools/check_traceability.py`, which CI runs on every pull request (since LHU-006). Any other row must point at a tagged test that exists, or at a measurement script or manual procedure file that exists; a tagged test whose row still says Planned fails the check, so the row is updated in the pull request that adds the test.

Design elements are named as in `docs/architecture/ARCHITECTURE.md`. Tests are named by the file that will carry the tag `// Verifies: REQ-nnn` (see `docs/test/TEST_STRATEGY.md` section 5). Every ticket is numbered since plan revision 6; the milestone each ticket belongs to is on the GitHub board.

## Matrix

| Requirement | Design element | Ticket | Test or measurement | Status |
|---|---|---|---|---|
| REQ-001 | `CommandAllowlist`, `Elm327Protocol` (single guarded send path, `src/hardware/obd/`); `CanFrameReader`, `SocketCanFrameReader`, `SocketCanDbcSource` (no transmit member, no transmit call) | LHU-010; LHU-028 | `tests/unit/obd/command_allowlist_test.cpp` (all 256 modes through the protocol onto a fake transport: 3 written, 253 refused with 0 bytes, Mode 04 named); `tests/unit/obd/elm327_protocol_test.cpp`; `tests/unit/can/socket_can_dbc_source_test.cpp` (compile-time: no write, send or transmit member on any CAN type); CI step `tools/check_can_read_only.py` (no write, send, sendto, sendmsg, writev or pwrite call under `src/hardware/can/`) | OBD side merged with LHU-010; CAN side merged with LHU-028 |
| REQ-002 | `VehicleDataSource` interface, `SignalStoreFeeder`, `FakeSource`; source selection by configuration (LHU-012); the source only in the vehicle-data service process (LHU-022) | LHU-008; LHU-012; LHU-022; LHU-028; LHU-029 | `tests/integration/service_against_each_source_test.cpp` (parameterised by source harness: fake now, ELM327, CAN and replay as they land); `tests/unit/hardware/fake_source_test.cpp`; `tests/unit/service/signal_store_feeder_test.cpp`; CI step `tools/check_link_graph.py` on CMake's graphviz output (guards the D-Bus library too); the CAN harness (`tests/integration/can_source_harness.h`, fake frame reader) in the same suite; `tests/vcan/vcan_source_test.cpp` on the Pi; `tests/integration/vehicle_data_dbus_test.cpp` (the service executable with the source chosen on its command line) | Tagged tests and the CI link check merged with LHU-008; the process boundary merged with LHU-022; the ELM327 harness over the emulator merged with LHU-012 (`tests/integration/elm327_source_harness.h`); CAN harness merged with LHU-028; replay is proven by its own record-and-replay test instead of the suite, because playback cannot produce a chosen value on demand (DN-029) |
| REQ-003 | `SignalSample`, `SignalStore`, `Clock` | LHU-006 | `tests/unit/service/signal_sample_test.cpp`, `tests/unit/service/signal_store_test.cpp`, `tests/unit/service/staleness_monitor_test.cpp`, `tests/unit/service/steady_clock_test.cpp` | Tagged tests merged with LHU-006 |
| REQ-004 | `decodePid` and `SupportedPidSet` (`src/hardware/obd/`); PID discovery in `Elm327ObdSource` | LHU-009; LHU-012 | `tests/unit/obd/obd_pid_decoder_test.cpp` (40 hand-computed vectors, 5 per PID); `tests/unit/obd/supported_pid_set_test.cpp` (bitmaps); `tests/integration/pid_discovery_test.cpp` (unsupported PID never requested over 100 cycles) | Decoder and bitmap tests (LHU-009), the source unit tests and the discovery test over the emulator (LHU-012, `012F` never requested over 100 cycles, 0 forbidden requests) tagged and merged |
| REQ-005 | `DbcDatabase`, `DbcDecoder` (`src/hardware/can/`) with `dbc/simulated_vehicle.dbc` (an invented vehicle) (DN-027) | LHU-027 | `tests/unit/can/dbc_oracle_test.cpp` against `cantools` 44.2.1 (`tools/dbc_oracle.py`, test-only environment): 10,000 frames, 42,558 values, all 17 signals, every mismatch printed in full; `tests/unit/can/dbc_decoder_test.cpp` (hand-computed vectors for both byte orders, signed, 1 and 32 bits, scale and offset; parser errors) | Tagged and merged (LHU-027) |
| REQ-006 | `StalenessMonitor`, `SignalDefinition` (per-signal timeout), `Clock` | LHU-006; LHU-012 (scenarios); LHU-013 and LHU-039 (styling) | `tests/unit/service/staleness_monitor_test.cpp`, `tests/unit/service/signal_store_test.cpp`; `tests/scenarios/fault_injection_scenarios_test.cpp` (stale PID, ignition off); `tests/hmi/tst_home_screen.qml`, `tests/hmi/tst_vehicle_data_screen.qml` (stale never drawn in the live colour on both screens) | Unit tests (LHU-006), scenarios (LHU-012) and the HMI tests of both screens (LHU-013, LHU-039) tagged and merged |
| REQ-007 | `ConnectionStateMachine` with the transition table of DN-007 | LHU-007; LHU-012 (driven by the source); LHU-013 (display within 500 ms) | `tests/unit/service/connection_state_machine_test.cpp` (all 24 state and trigger pairs: 8 accepted, 16 rejected); `tests/scenarios/fault_injection_scenarios_test.cpp`; `tests/hmi/tst_home_screen.qml` (state shown within 500 ms); `tests/unit/hmi/view_models_test.cpp` | Unit test (LHU-007), scenarios (LHU-012) and the HMI timing test (LHU-013) tagged and merged |
| REQ-008 | `ReconnectBackoff` and the loss detection and reconnect loop in `Elm327ObdSource` | LHU-012 | `tests/unit/elm327/reconnect_backoff_test.cpp`; `tests/unit/elm327/elm327_obd_source_test.cpp` (schedule on the manual clock); `tests/scenarios/reconnect_after_emulator_restart_test.cpp` (20 kills and restarts in real time; 1, 2, 4, 8, 10, 10 s intervals within 10%); `tests/scenarios/fault_injection_scenarios_test.cpp` (silent adapter) | Tagged tests merged with LHU-012 |
| REQ-009 | Whole path, source to rendered frame, including the D-Bus hop | LHU-032; LHU-022 (hop measured) | `tools/measure/measure_latency.py`; raw data in `docs/measurements/latency/` | Planned |
| REQ-010 | `Elm327Protocol`, `decodePid`, `DbcDecoder` | LHU-010; LHU-009; LHU-027 | `tests/unit/obd/obd_pid_decoder_test.cpp` (wrong byte counts, unknown PIDs, 10,000 random pairs); `tests/unit/obd/elm327_protocol_malformed_input_test.cpp` (named corpus of 16 replies plus 100,000 random strings, run under sanitizers in CI); `tests/unit/can/dbc_decoder_test.cpp` (named malformed-frame corpus and 100,000 random frames, run under sanitizers in CI) | Decoder (LHU-009), parser (LHU-010) and the source's handling of named errors, truncated and corrupt replies over the emulator (LHU-012, `tests/scenarios/fault_injection_scenarios_test.cpp`) tagged and merged; the DBC part merged with LHU-027 |
| REQ-011 | View models (`src/hmi/viewmodels/`) as the only QML binding target; `WorkerBridge` as the thread hop; `VehicleDataClient` as the only path from a view model to vehicle data from v0.2.0 | LHU-013; LHU-022 | CI steps: `tools/check_hmi_includes.py` (no hardware header, only value-type service headers, Qt-only QML imports) and `tools/check_link_graph.py` (HMI targets reach no source); `tests/unit/hmi/view_models_test.cpp` (thread hop) | CI checks and the thread-hop test merged with LHU-013; the vehicle-data app reads the D-Bus client with `--source dbus` (LHU-022), with the view models unchanged |
| REQ-012 | QML home and vehicle-data screens, `SignalTileModel`, `ConnectionStatusModel`, `VehicleDataViewModel` | LHU-013 (home); LHU-039 (vehicle data) | `tests/hmi/tst_home_screen.qml` (value, unit, status and styling for the two primary signals; millimetre sizes); `tests/unit/hmi/view_models_test.cpp`; `tests/hmi/tst_vehicle_data_screen.qml` (all eight signals, sizes, the Home button); `tests/hmi/tst_screens.qml` (navigation) | Both screens tagged and merged (LHU-013, LHU-039) |
| REQ-013 | Service and hub startup, systemd units | LHU-032 | `tools/measure/measure_boot_time.py`; raw data in `docs/measurements/boot_time/` | Planned, target provisional |
| REQ-014 | Every process: service, hub, vehicle-data app, browser | LHU-032 | `tools/measure/measure_memory.py` (per process); raw data in `docs/measurements/memory/` | Planned, target provisional, to be re-set per process |
| REQ-015 | `RecordingByteTransport`, `ReplayByteTransport`, `ReplayClock`, `ReplaySource` (`src/hardware/replay/`), `tools/scrub_recording.py` (DN-029) | LHU-029 | `tests/integration/record_replay_test.cpp` (a live session against the emulator recorded through the full source, replayed fast: identical samples in signal, value, unit, status and order, identical transitions, 0 divergences); `tests/unit/replay/recording_test.cpp` (format, replay transport, divergence); `tools/test_scrub_recording.py` (VIN in plain text and in Mode 09 hexadecimal with and without spaces, over several frames and reads; nothing written if a VIN remains) | Tagged and merged (LHU-029) |
| REQ-016 | `AppRegistry`, `AppProcessManager` over `PosixProcessLauncher`, `HubViewModel`, `HubControlServer`, the `LexusHub` QML grid (DN-021) | LHU-021 (with the LHU-020 question) | `tests/integration/hub_process_cycle_test.cpp` (20 real start-and-exit cycles, each seen within 1 s, same process ID, nothing left unreaped; process-group kill); `tests/integration/hub_end_to_end_test.cpp` (the `lexus-hub` executable: 20 cycles through the control socket, idle with a visible window within 1 s, same hub process ID, stop on SIGTERM); `tests/hmi_hub/tst_hub_screen.qml` (8 apps give 8 tiles at or above 10 mm, tap launches, grid disabled while an app runs); `tests/unit/hub/` and `tests/unit/hmi/hub_view_model_test.cpp`; stacking on the panel in `docs/release/PI_BRINGUP_CHECKLIST.md` step 3.5 | Desk part tagged and merged (LHU-021); the panel part is a Pi step (A10 to A12) |
| REQ-017 | `VehicleDataService` (D-Bus adapter over the service layer), `VehicleDataClient`, the interface file `src/service_dbus/interface/io.github.samersamaha.LexusHeadUnit.VehicleData1.xml`, the executable `lexus-vehicle-data-service` (DN-022) | LHU-022 | `tests/integration/vehicle_data_dbus_test.cpp` on a private bus (two clients, identical 1,000-sample sequences with the hop printed; a late client complete within 500 ms; transitions; service gone and back; name already owned; live introspection equals the file; the executable end to end); `tests/unit/service_dbus/dbus_types_test.cpp`; the hop on the Pi is part of LHU-032 | Desk part tagged and merged (LHU-022); the hop on the Pi is not yet measured |
| REQ-018 | URL entries in `AppRegistry` (LHU-021); the browser command and the three test apps in `deploy/hub.conf` (LHU-023) | LHU-023 | `docs/test/MANUAL_WEB_APPS_PROCEDURE.md` (video, protected audio with the DRM module recorded, a touch game, return, tracking); `tools/measure/sample_process_memory.py` (RSS and PSS of the browser's process tree every 10 s for 10 min, unit-tested on a fake `/proc`); results in `docs/test/results/` and `docs/measurements/memory/` | Planned: procedure and tool merged (LHU-023); no result until the Pi run |
| REQ-019 | Operating-system audio configuration in `deploy/`; connection log of `ConnectionStateMachine` | LHU-024 | `docs/test/MANUAL_ON_CAR_PROCEDURE.md` (audio section); results and logs in `docs/test/results/` | Planned |
| REQ-020 | `decodeGetThrottled` and `PowerFlags` (service library); `PowerStatusReader` (`vcgencmd` through `QProcess`, and a fake), `PowerStatusModel`, `ShutdownController` and the hub status strip (DN-025) | LHU-025 | `tests/unit/service/power_status_test.cpp` (every bit, the logger's 0x50005 example, malformed outputs); `tests/unit/hmi/power_status_model_test.cpp` (each flag set and cleared, unavailable, skipped polls; two-tap shutdown, expiry, failure); `tests/hmi_hub/tst_hub_power.qml` (each current flag on the strip within 5 s and cleared, the 10 mm shutdown button needing two taps); 10 shutdown cycles on the Pi in `docs/test/results/` (checklist step 3.8) | Desk part tagged and merged (LHU-025); the shutdown cycles are a Pi step |
| REQ-021 | `DtcDecoder`, `VehicleInfoDecoder`, `DiagnosticsModel`, diagnostics screen | LHU-030 | `tests/unit/obd/dtc_decoder_test.cpp` (0, 1, 2, 6 codes; P, C, B, U; multi-frame); `tests/unit/obd/vehicle_info_decoder_test.cpp`; `tests/hmi/diagnostics_screen_test.qml` | Planned |
| REQ-022 | `DerivedSignalEngine` and its derived-signal definitions | LHU-031 | `tests/unit/service/fuel_economy_test.cpp`, `tests/unit/service/trip_totals_test.cpp`; `tests/integration/replay_deterministic_totals_test.cpp` | Planned |

## Coverage summary

| | Count |
|---|---|
| Requirements | 22 |
| With a named design element | 22 |
| With a named ticket | 22 |
| With a named test or measurement | 22 |
| With a test that exists and is tagged | 15 (REQ-001, REQ-002, REQ-003, REQ-004, REQ-005, REQ-006, REQ-007, REQ-008, REQ-010, REQ-011, REQ-012, REQ-015, REQ-016, REQ-017, REQ-020) |

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
| LHU-020 | none; its desk question is answered in DN-021, its Pi part is checklist step 3.5 |
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
