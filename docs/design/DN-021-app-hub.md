# DN-021: App hub launcher

| | |
|---|---|
| Ticket | LHU-021 (with the return-to-hub question of LHU-020) |
| Requirements | REQ-016 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-08, about 40 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

The head unit needs a full-screen launcher that shows the configured apps as touch targets, starts the selected one as a separate operating-system process, tracks it, and is visible again within 1 s of that process exiting, without the launcher itself ever exiting (REQ-016). Under the labwc Wayland compositor a client cannot raise itself above another window, so the design also has to give the user a way back to the hub while an app is still running (OQ-29), and that way can only be verified on the Pi.

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | How does the user get back to the hub while an app runs (OQ-29)? The LHU-020 spike needs the Pi | **Return means stopping the foreground app.** The hub cannot raise itself under Wayland, but it is the window underneath, so when the app's process group is gone the hub is what the compositor shows. The hub listens on a local socket; `lexus-hub --send return` asks it to stop the foreground app. On the Pi that command is a launcher on the Raspberry Pi OS panel, which stays visible because apps are started maximised rather than full screen (new assumption A11). Fallbacks, in order: a labwc key or touch binding running the same command; a layer-shell overlay bar (needs the `layer-shell-qt` package, which is not installed and would need approval). The spike's other questions (memory while a video plays) stay with LHU-020 as a Pi step |
| 2 | What is the registry file? | The existing `key = value` format with sections (`KeyValueConfiguration`, DN-012), so there is one parser in the project. `[hub] apps = id, id, ...` gives the order; each app is a section `[app.<id>]` with `name`, `kind` (`native` or `url`), `command` (native) or `url` (url), optional `icon` (image path) and optional `restart` (`never`, the default, or `on-failure`). `[hub] browser_command` is the command line for URL apps and must contain `{url}`. Ships as `deploy/hub.conf` |
| 3 | Is the command line run through a shell? | No. `splitCommandLine` turns it into an argument vector: whitespace separates, double quotes group, backslash escapes inside quotes. No variables, globbing or pipes, so a registry entry cannot run more than one program |
| 4 | How many apps run at once? | One foreground app. A launch while an app runs is refused (`AlreadyRunning`); the grid is only reachable when no app covers it. Background services (the vehicle-data service, LHU-022) are systemd units, not hub apps |
| 5 | How is a process started and watched? | `posix_spawnp` (path search, inherits the environment), in a new process group so that a stop reaches the app's own children too (a browser starts several). Exit is collected with `waitpid(pid, WNOHANG)` on that one process ID only, from a 100 ms timer in the hub. 100 ms polling keeps the exit-to-visible time well inside 1 s without signal handlers |
| 6 | How is an app stopped? | `SIGTERM` to the process group, then `SIGKILL` to the group if it has not exited after a grace period (default 3 s) |
| 7 | What is the restart policy? | `on-failure` restarts an app that exited with a non-zero code or by a signal it was not sent by the hub, at most 3 times within 60 s; after that the hub stays visible and shows the cause. `never` (the default) just reports the exit. A clean exit (code 0) or a stop the hub asked for is never restarted |
| 8 | What does a URL app run? | `browser_command` with `{url}` replaced, split the same way. Assumption A10: Chromium started with its own `--user-data-dir` stays as the launched process rather than handing the URL to an existing instance and exiting, so the hub can track it. Web apps proper are LHU-023 |
| 9 | What happens when the hub process itself ends (window closed, signal)? | It stops the foreground app first (same term-then-kill sequence), so no app outlives it. Restarting the hub after a crash is the systemd unit's job (LHU-022) |
| 10 | Does the hub show vehicle data now? | No. The status strip shows the hub's own state (foreground app, last exit cause). Connection state and power flags come from `VehicleDataClient` (LHU-022) and `PowerStatusProvider` (LHU-025) |
| 11 | What is "the hub is the foreground window within 1 s" at the desk? | At the desk (offscreen, no compositor) it is measured as: the hub reports no foreground app and its window is visible within 1 s of the app process exiting. Stacking on the panel is assumption A12, verified on the Pi |

## 3. Nouns to classes

Plain C++17, library `lexus_head_unit_hub_core` (`src/hub/core/`), no Qt (D-008):

| Class | Responsibility |
|---|---|
| `AppEntry` (struct) | One registry entry: id, name, icon, kind, argument vector, restart policy |
| `AppRegistry` | Builds the ordered list of entries from a `KeyValueConfiguration`; collects one error string per rejected entry |
| `splitCommandLine` (free function) | Command text to argument vector, or nothing if the quotes do not close |
| `ProcessLauncher` (interface) | Start an argument vector in a new process group, poll one process for exit without blocking, signal a process group |
| `PosixProcessLauncher` | `ProcessLauncher` over `posix_spawnp`, `waitpid` and `kill` |
| `AppProcessManager` | The one foreground app: launch, stop with escalation, exit detection, restart policy, counters, exit reports |

Qt, library `lexus_head_unit_hub_viewmodels` (`src/hub/viewmodels/`):

| Class | Responsibility |
|---|---|
| `AppListModel` (`QAbstractListModel`) | The registry entries as rows with roles `appId`, `name`, `icon` |
| `HubViewModel` | What QML binds to: the app list, `appRunning`, `foregroundAppName`, `lastExitText`, `launch(row)`, `requestReturn()`; drives `AppProcessManager::poll()` from a 100 ms timer |
| `HubControlServer` | `QLocalServer` taking one-line commands: `launch <id>`, `return`, `status` |

QML module `LexusHub` (`src/hub/qml/`): `Main`, `HubScreen`, `AppTile`, `HubStatusStrip`, and the same `Sizes` singleton file as the vehicle-data app. Executable `lexus-hub` (`src/hub/app/`).

## 4. What each class stores

- `AppEntry`: `std::string id, name, icon`; `AppKind kind`; `std::vector<std::string> arguments`; `RestartPolicy restart`.
- `AppRegistry`: `std::vector<AppEntry> m_entries`; `std::vector<std::string> m_errors`.
- `PosixProcessLauncher`: nothing.
- `AppProcessManager`: `ProcessLauncher*`, `const Clock*`, `AppManagerSettings` (grace 3000 ms, window 60000 ms, 3 restarts); the running entry (`std::optional<AppEntry>`), its process ID, start time, run state (Idle, Running, Stopping), the stop deadline, the timestamps of recent restarts, counters (launches, exits, restarts, start failures, kills), the last exit report and an exit listener.
- `AppListModel`: a copy of the entries.
- `HubViewModel`: `AppProcessManager*`, `AppListModel`, `QTimer`, cached `lastExitText`.
- `HubControlServer`: `QLocalServer`, `HubViewModel*`.

## 5. Verbs to methods

| Class | Method | Inputs | Output | Notes |
|---|---|---|---|---|
| free | `splitCommandLine(text)` | `std::string_view` | `std::optional<std::vector<std::string>>` | Empty optional on an unclosed quote |
| `AppRegistry` | `fromConfiguration(configuration)` | `KeyValueConfiguration` | `AppRegistry` | Never throws; bad entries go to `errors()` |
| `AppRegistry` | `entries()`, `errors()`, `find(id)` | | list, list, pointer or null | |
| `ProcessLauncher` | `start(arguments)` | argument vector | `std::optional<ProcessId>` | New process group; empty optional if the program cannot be run |
| `ProcessLauncher` | `pollExit(processId)` | process ID | `std::optional<ProcessExit>` | Non-blocking; reaps the process when it has exited |
| `ProcessLauncher` | `terminateGroup`, `killGroup` | process ID | `bool` | `SIGTERM` or `SIGKILL` to the group |
| `AppProcessManager` | `launch(entry)` | `AppEntry` | `LaunchResult` (Started, AlreadyRunning, StartFailed, EmptyCommand) | |
| `AppProcessManager` | `requestStop()` | | `bool` (false when idle) | Sends `SIGTERM`, sets the deadline |
| `AppProcessManager` | `poll()` | | | Reaps, escalates to `SIGKILL` after the grace period, applies the restart policy, calls the exit listener |
| `AppProcessManager` | `shutdown(waitMilliseconds)` | ms | | Stop and wait; used when the hub ends |
| `AppProcessManager` | `runState()`, `foregroundAppId()`, `foregroundProcessId()`, `counters()`, `lastExit()` | | | |
| `HubViewModel` | `launch(row)`, `launchById(id)`, `requestReturn()` | row or id | `bool` | `Q_INVOKABLE` |

An exit report holds the app id, the cause (CleanExit, FailedExit with the code, Signalled with the signal, StoppedByHub, StartFailed), the run time in milliseconds and whether a restart follows.

## 6. Interaction sequence (main scenario)

1. `lexus-hub --registry deploy/hub.conf` reads the registry, builds the manager over `PosixProcessLauncher`, the view model, the control server, and loads `LexusHub/Main`.
2. The user taps a tile; QML calls `hub.launch(row)`; the manager spawns the command line in a new process group and enters Running; the view model sets `appRunning`.
3. The app's window covers the hub.
4. The user taps the panel's Hub launcher, which runs `lexus-hub --send return`; the control server calls `requestReturn()`; the manager sends `SIGTERM` to the group (Stopping).
5. Within 100 ms of the process exiting, the timer's `poll()` reaps it; the manager reports StoppedByHub and returns to Idle; `appRunning` turns false; the compositor shows the hub, which never moved.

If the app exits on its own, steps 4 and the stop are skipped and the cause is CleanExit, FailedExit or Signalled; with `on-failure` and budget left, the manager starts it again in the same `poll()`.

## 7. Failure cases

| Failure | Handling |
|---|---|
| Registry file missing | Hub starts with an empty grid and the error in the status strip; it does not exit |
| Bad entry (unknown kind, no command, unclosed quote, duplicate id, URL app with no `{url}` in `browser_command`) | Entry skipped, error recorded and printed; the other entries load |
| Program not found or not executable | `posix_spawnp` fails: StartFailed report, start-failure counter, hub stays visible |
| App ignores `SIGTERM` | `SIGKILL` to the group after the grace period; kill counter |
| App crashes repeatedly | `on-failure` stops after 3 restarts in 60 s and reports the last cause |
| A second launch while an app runs | Refused with AlreadyRunning |
| Control socket name in use (a second hub) | The second hub prints the error and exits with code 2 before showing a window; the first keeps running |
| Unknown control command | Reply `error unknown command`; nothing changes |
| Hub ends | `shutdown()` stops the foreground app first |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | `splitCommandLine`: words, quotes, escapes, empty input, unclosed quote | REQ-016 |
| 2 | `AppRegistry`: order from `hub.apps`; native and URL entries; every rejected-entry case of section 7 with its error; default restart policy | REQ-016 |
| 3 | `AppProcessManager` with a fake launcher and a manual clock: launch, AlreadyRunning, StartFailed, each exit cause, stop with escalation to kill at exactly the grace period, restart policy (3 restarts in 60 s, then none; budget recovers after the window), shutdown | REQ-016 |
| 4 | Integration, real processes: `PosixProcessLauncher` with a fake app program, 20 start-and-exit cycles; each exit seen within 1 s; the test's process ID unchanged; no child left; a stop of an app that ignores `SIGTERM` ends in `SIGKILL` | REQ-016 |
| 5 | Integration, end to end: the `lexus-hub` executable offscreen with a registry naming the fake app; 20 cycles driven through the control socket; after each exit the hub reports no foreground app and a visible window within 1 s; the hub's process ID is the same across all 20 cycles and it is still running | REQ-016 |
| 6 | HMI: a registry of 8 apps renders 8 tiles, each at least 10 mm on a side; a tap calls `launch` with the tile's row; the grid is disabled while an app runs | REQ-016 |

On the Pi (checklist step 3.5): the panel launcher returns to the hub by touch; the hub is visible within 1 s of an app exiting (A11, A12); a URL app is tracked (A10).

## 9. Alternatives considered

| Alternative | Why rejected |
|---|---|
| `QProcess` for the process manager | Puts Qt into the process logic, against D-008, and hides the process group and signal escalation that the ticket is meant to show; the manager is unit-tested without an event loop |
| `system()` or `sh -c` for the command | One registry line could run anything the shell allows; quoting bugs become injection bugs |
| `SIGCHLD` handler instead of polling | Correct but harder: async-signal-safe code, a self-pipe into the event loop, and it would also see children the hub did not start through the manager. 100 ms polling of one process ID meets the 1 s target with a simpler failure surface |
| Hub raises itself on return | Wayland does not allow it without an activation token from the compositor; stopping the app gives the same result without depending on compositor behaviour |
| Kiosk-mode browser | No way back to the hub without a keyboard; rejected for the in-car use |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #60. No deviation from the interface or the sequence. Details the note did not spell out:

- The registry gained `hub.stop_grace_ms` (default 3000), so the end-to-end test can use 500 ms.
- `HubViewModel::launchById` returns the result as text, because its only caller is the control socket.
- The `status` reply carries `window visible|hidden`: the desk stand-in for "the hub is in front" (question 11).
- SIGTERM and SIGINT reach the hub through a socket pair and a `QSocketNotifier` (the self-pipe pattern), so `main()` runs `shutdown()` and no app outlives the hub; the end-to-end test sends SIGTERM and checks the app is gone.
- The `Sizes.qml` file of the vehicle-data app is compiled into the `LexusHub` module too, so both apps use one set of millimetre rules.
- The fake app fixture (`tests/fixtures/fake_app.cpp`) can ignore SIGTERM and start a grandchild, which is how the process-group kill is tested.

Desk timing (20 cycles each, from the test output): exit seen 100 ms after launch for an app that runs 50 ms (min, median and max all 100 ms, set by the 100 ms poll); through the `lexus-hub` executable, launch to idle with a visible window for an app that runs 100 ms: min 103, median 198, max 209 ms (debug) and min 183, median 195, max 204 ms (sanitizers). On the panel: not yet measured (checklist step 3.5). Status: Implemented.
