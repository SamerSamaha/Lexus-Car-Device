# DN-029: Record and replay with vehicle-identification scrub

| | |
|---|---|
| Ticket | LHU-029 |
| Requirements | REQ-015 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-09, about 40 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-09 |

## 1. Problem

A drive happens once; its bugs must be reproducible at the desk. The raw bytes between the ELM327 source and the adapter are recorded with timestamps, and a `ReplaySource` plays them back through the unchanged source and service layer to produce the same samples in the same order (REQ-015). A recording can contain the vehicle identification number, so a scrub step removes it, in every encoding, before a recording may leave the ignored `local_recordings/` folder (D-023, OQ-21).

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | At which level is recorded? | `ByteTransport`, the lowest level the code owns. A `RecordingByteTransport` wraps the real one and writes every call and result to the file. Recording samples instead would test nothing below the store; recording bytes replays the parser, the protocol, discovery, the poll loop and loss detection |
| 2 | File format? | Text, one event per line, so a recording can be read and diffed: `<milliseconds> <event> [<hex bytes>]`, events `OPEN_OK`, `OPEN_FAIL`, `CLOSE`, `WRITE`, `READ` (with bytes), `READ_TIMEOUT`, `READ_CLOSED`, `READ_ERROR`. A first line `# lexus-head-unit recording 1 source=elm327` names the format version. Timestamps are the source clock's milliseconds |
| 3 | How does replay keep the source's timing logic (reply timeouts, 2 s loss, backoff) the same? | A `ReplayClock` whose time is the recorded time of the event most recently consumed. The source runs on that clock, so every decision it makes with the clock sees the times it saw while recording, and the samples even get the recorded timestamps |
| 4 | What if the replayed source writes something different from the recording (the code changed)? | That is a divergence: counted, and from then on reads return Error, so the source goes to Error and the test fails loudly. Replay of an old recording against new code is exactly where this is wanted |
| 5 | What happens at the end of the recording? | Reads return Closed (end of file, as when the adapter disappears); the replay source then reports finished and stops retrying |
| 6 | "With original timing"? | `replay.timing = original` paces playback by the recorded times: `idleHintMilliseconds()` returns the wait until the next event is due on the wall clock, so the worker loop sleeps. `fast` (tests) plays as fast as possible |
| 7 | Where does the VIN appear in a recording? | In a Mode 09 PID 02 reply, as the adapter's ASCII text of hexadecimal bytes ("49 02 01 31 47 31 ..." or without spaces after `ATS0`), possibly split over several lines with frame counters, inside a `READ` line that is itself hex-encoded by the recording. The scrub decodes the recording's hex to the adapter's text, takes every run of hexadecimal byte pairs, decodes those to characters, and looks for a VIN-shaped run of 17 characters (the same shape as `tools/check_private_data.py`); it also looks in the plain text. Every character of a VIN found is replaced by `0` (the byte 0x30), in every encoding, so lengths and line structure stay the same |
| 8 | How is the scrub checked? | After scrubbing, the tool scans all three forms again (plain, hex-decoded, recording-decoded) and refuses to write the output if anything VIN-shaped remains. `tools/check_private_data.py` still guards the repository itself |
| 9 | How is a recording made in practice? | `lexus-head-unit --record local_recordings/<name>.rec` (and the same option on the service) wraps the ELM327 transport. Playback is `source.kind = replay` with `replay.file` and `replay.timing` |
| 10 | Is replay part of the REQ-002 suite? | No: the suite asks a source to produce a chosen value or to lose its link on demand, which a playback cannot do. Replay is proven by its own integration test against a recording made live against the emulator |

## 3. Nouns to classes

| Class | Responsibility |
|---|---|
| `RecordingEvent` (struct), `RecordingEventKind` | One line of a recording |
| `RecordingWriter` / `readRecording` | Append events to a file; parse a file into events with line-numbered errors |
| `RecordingByteTransport` | A `ByteTransport` decorator that records every call on the wrapped transport |
| `ReplayClock` | A `Clock` whose time is set by the replay transport |
| `ReplayByteTransport` | A `ByteTransport` that answers from a recording and checks the writes |
| `ReplaySource` | A `VehicleDataSource` that owns the replay transport, the clock and an `Elm327ObdSource`, and paces it |
| `tools/scrub_recording.py` | The VIN scrub with its own verification |

## 4. What each class stores

- `RecordingEvent`: `std::int64_t timestampMilliseconds`, `RecordingEventKind kind`, `std::vector<std::uint8_t> bytes`.
- `RecordingByteTransport`: the wrapped transport, the clock, the output file stream, an event counter.
- `ReplayByteTransport`: the events, the next index, the clock to advance, divergence and write counters, a pending-read buffer for a recorded chunk larger than the caller's `maxBytes`.
- `ReplaySource`: the clock, the transport, the inner `Elm327ObdSource`, the timing mode, the wall-clock start, a finished flag.

## 5. Verbs to methods

| Class | Method | Notes |
|---|---|---|
| `RecordingByteTransport` | the `ByteTransport` methods | Each forwards, then appends one event with the clock's time |
| | `eventCount()` | |
| free | `readRecording(path) -> RecordingFile {events, errors}` | Never throws |
| `ReplayByteTransport` | the `ByteTransport` methods | `write` compares with the next `WRITE`; `read` returns the next `READ*` event; each advances the `ReplayClock` |
| | `divergenceCount()`, `eventsRemaining()`, `finished()` | |
| `ReplaySource` | the `VehicleDataSource` methods; `finished()` | `runOnce` forwards to the inner source unless finished; `idleHintMilliseconds` paces original timing |

## 6. Interaction sequence

Recording: the app builds `FileDescriptorByteTransport`, wraps it in `RecordingByteTransport` (file under `local_recordings/`), and gives the wrapper to `Elm327ObdSource`. Every open, write and read goes into the file.

Replay: the app builds `ReplaySource` from the file; `start()` starts the inner source on the replay transport; each `runOnce()` lets the inner source poll, and the transport hands it the recorded bytes and sets the clock to the recorded time; samples flow to the store as live. At the end of the file the inner source sees Closed and goes to Error; `ReplaySource` reports finished.

## 7. Failure cases

| Failure | Handling |
|---|---|
| Recording file missing or malformed | `readRecording` errors with line numbers; `ReplaySource` stays Disconnected and reports them |
| A write differs from the recording | Divergence counted; reads return Error from then on |
| End of recording | Reads return Closed; finished |
| Recorded chunk larger than the caller's buffer | The rest is kept and returned by the next read without consuming another event |
| VIN left after scrubbing | The scrub tool refuses to write the output and exits 1 |
| Recording a session that fails to write the file | The recorder counts write failures; the session itself continues |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | Recording format: every event kind round-trips through write and read; malformed lines give numbered errors | REQ-015 |
| 2 | Replay transport: writes checked, reads in order, clock follows the events, chunk splitting, divergence, end of file | REQ-015 |
| 3 | Integration: a session against the emulator recorded live through the full service layer, then replayed fast; the replayed samples equal the live ones in signal, value, unit, status and order (and timestamps, reported) | REQ-015 |
| 4 | Scrub: a synthetic recording with a VIN in Mode 09 replies (with and without spaces, multi-line) and in plain text is scrubbed in all forms, lengths kept, verification passes; an unscrubbable input is refused | REQ-015 |

## 9. Alternatives considered

| Alternative | Why rejected |
|---|---|
| Record decoded samples | Replays only the store and the screens; the parser, protocol and loss detection would not be exercised |
| Binary format | Smaller, but a recording would need a tool to read; text diffs show what changed between two drives |
| Replay on the steady clock with sleeps | Timing decisions in the source would see different times from the recording and could diverge (a reply arriving "late"); the replay clock removes that |
| Scrub by deleting Mode 09 lines | Changes the event sequence, so the scrubbed recording would no longer replay |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #64. Deviations and details:

- **WRITE lines carry the accepted count** ("<ms> WRITE <hex> <count>"). The first version recorded only the bytes a transport accepted; after the emulator died a write was accepted as 0 bytes, so the replay saw a 5-byte write where the recording had an empty one and counted a divergence. The integration test found it on its first run.
- `KeyValueConfiguration` gained `setValue`, so `--record` (on the app and the service) sets `record.file` without changing `buildSource`.
- A `CLOSE` at the head of the recording while the source is not connected is the `stop()` that ended the session; the replay consumes it instead of letting the source retry into it.
- The scrub imports the VIN shape from `tools/check_private_data.py`, so both use one definition.

Evidence at the desk: the record-and-replay test run 10 times in a row; every run 40 live samples, 6 transitions, 162 recorded events, and a replay identical in signal, value, unit, status and order with 0 divergences; samples with exactly the recorded timestamp 40, 38, 40, 40, 40, 40, 39, 39, 40, 40 of 40 (the rest differ by the millisecond in which the recorder and the source read the clock). A smoke run of the app recorded 4 s against the emulator (76,420 lines, 25,473 writes: the emulator answers far faster than an adapter) and replayed it. The scrub tests fail when the hexadecimal scrub is disabled (3 of 5). Status: Implemented.
