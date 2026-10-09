# DN-010: ELM327 response parser and command allowlist

| | |
|---|---|
| Ticket | LHU-010 |
| Requirements | REQ-001, REQ-010 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-08, about 25 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

Everything the head unit says to the car goes through one text line to an ELM327-compatible adapter, so this ticket is where "read-only" becomes code: a `CommandAllowlist` that decides whether a command may be sent at all, and an `Elm327Protocol` that sends an allowed command through a `ByteTransport`, reads the reply up to the `>` prompt with a timeout, and classifies it so that no malformed or error reply can ever be mistaken for data. The source of LHU-012 is the only caller.

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | What exactly is allowed? | A fixed list of base ELM327 `AT` setup commands (`ATZ`, `ATE0`, `ATL0`, `ATS0`, `ATH0`, `ATSP0`, `ATI`, `ATRV`, `ATDPN`) and OBD requests of Mode 01 (`01` plus a two-digit PID), Mode 03 (`03` alone) and Mode 09 (`09` plus a two-digit PID). Nothing else: no `ST` commands, no other `AT` command, no other mode, no raw frame (D-009) |
| 2 | Is the check by prefix or by exact shape? | By exact shape: a command is allowed only if it is one of the listed `AT` strings or matches the mode grammar exactly (hexadecimal upper or lower case, 2 or 4 characters). `010D1` or `01 0D` are refused as malformed. The 256-mode test of REQ-001 offers every two-digit mode with a PID |
| 3 | What does the transport look like? | `ByteTransport`: `open()`, `close()`, `isOpen()`, `write(bytes)` returning the count written, `read(buffer, maxBytes, timeoutMs)` returning a count and a status (Ok, Timeout, Closed, Error). A fake with scripted replies is in this ticket; the serial device, pseudo-terminal and RFCOMM implementations come with LHU-012 |
| 4 | How is a reply delimited? | The adapter ends every reply with `\r` lines and a `>` prompt. The protocol reads until `>` or the deadline from the injected `Clock`. A reply longer than 4,096 bytes without a prompt is Malformed (a runaway adapter cannot grow memory) |
| 5 | What about echo and `SEARCHING...`? | Echo is turned off by `ATE0` but the first commands run before it; a first line equal to the command is dropped. `SEARCHING...` lines are dropped. Spaces are removed so the same parser works with `ATS0` on or off |
| 6 | What kinds of reply exist? | Data (one or more hexadecimal lines), Ok and Text (AT replies such as `OK`, `ELM327 v1.5`, `12.6V`, `A6`), and the named errors of REQ-010: `NO DATA`, `?`, `CAN ERROR`, `BUFFER FULL`, `STOPPED`, `UNABLE TO CONNECT`, plus `BUS INIT: ...ERROR`, `ERROR`, a negative response (`7F ...`), Timeout, LinkError (write failed or transport closed), Malformed (anything else where data was expected), Refused (the allowlist said no) |
| 7 | What does a Mode 01 reply look like to the decoder? | `mode01DataBytes(reply, pid)` finds the first line that starts with `41` and the PID and returns the bytes after it, or nothing. With `ATH0` there are no headers. If two modules answer, the first line wins. The caller passes the bytes to `decodePid` (DN-009), which checks the count |
| 8 | Who counts errors? | The protocol counts refused commands, timeouts, link errors and malformed replies; the source (LHU-012) counts malformed inputs in `SourceCounters` from the kinds it receives. Both are visible in tests |
| 9 | Which thread? | The worker thread, inside the source's `runOnce()`; every read is bounded by the timeout |

## 3. Nouns to classes

| Class | Responsibility |
|---|---|
| `CommandAllowlist` | `decide(command)` returns Allowed, RefusedMode, RefusedUnknownAtCommand or RefusedMalformed; `isAllowed(command)`; `elm327SetupCommands()` is the fixed list; `obdRequest(mode, pid)` builds a request text for an allowed mode |
| `ByteTransport` (interface) | One link: open, close, write bytes, read bytes with a timeout |
| `FakeByteTransport` | Scripted replies per command or queued raw bytes, records every write, can be closed to simulate link loss, can split replies into chunks |
| `Elm327Reply` (struct) | `kind`, `lines` (cleaned, upper case, no spaces), `rawText` |
| `Elm327Protocol` | `execute(command)`: allowlist, write, read until prompt or deadline, classify; counters |
| free functions | `hexToBytes(text)`, `classifyReplyLines(lines)`, `mode01DataBytes(reply, pid)` |

## 4. What each class stores

- `CommandAllowlist`: no state; the lists are `constexpr` tables.
- `FakeByteTransport`: `bool m_open`, `std::map<std::string, std::vector<std::string>> m_repliesByCommand` (a queue per command), `std::deque<std::uint8_t> m_pendingBytes`, `std::vector<std::string> m_writtenCommands`, `std::size_t m_totalBytesWritten`, `std::size_t m_chunkSize`.
- `Elm327Protocol`: constructor takes `ByteTransport&`, `const Clock&`, `std::int64_t replyTimeoutMilliseconds` (default 2000, configurable by LHU-012); non-owning pointers; counters `refusedCommandCount`, `timeoutCount`, `linkErrorCount`, `malformedReplyCount`, `commandsSent`. Owned by the source.

## 5. Verbs to methods

| Class | Method | Inputs (with units) | Output (with units) | Notes |
|---|---|---|---|---|
| `CommandAllowlist` | `decide(command)` | text | `AllowlistDecision` | Pure; upper-cases a copy before matching |
| `CommandAllowlist` | `obdRequest(mode, pid)` | mode byte, PID byte | `std::optional<std::string>` | Only for modes 0x01, 0x03 (no PID), 0x09; empty otherwise, so no code path can build a Mode 04 text |
| `Elm327Protocol` | `execute(command)` | text without `\r` | `Elm327Reply` | Refused replies write 0 bytes; the test of REQ-001 asserts the transport saw nothing |
| `Elm327Protocol` | counters | — | counts | |
| `ByteTransport` | `write(bytes)` | bytes | count written | 0 means the link failed |
| `ByteTransport` | `read(buffer, maxBytes, timeoutMs)` | buffer, limit, timeout ms | `ReadResult{count, status}` | Returns as soon as any bytes arrive, or at the timeout |
| free | `mode01DataBytes(reply, pid)` | reply, PID byte | `std::optional<std::vector<std::uint8_t>>` | Empty unless `kind == Data` and a line starts with `41` + PID and the rest is whole hexadecimal bytes |

## 6. Interaction sequence, main scenario

1. The source calls `protocol.execute("010D")`.
2. The allowlist says Allowed (Mode 01, PID 0D). The protocol writes `010D\r` and records one command sent.
3. It reads in chunks until `>` arrives: `410D3C\r\r>`. Elapsed time is checked against the deadline on every read.
4. The text is split on `\r`, spaces removed, upper-cased, empty lines and `SEARCHING...` dropped, the echo dropped if present; the remaining line `410D3C` is hexadecimal, so the kind is Data.
5. The source calls `mode01DataBytes(reply, 0x0D)`, gets `{0x3C}`, and `decodePid(0x0D, {0x3C})` gives 60 km/h (DN-009), which becomes a sample (LHU-012).

## 7. Failure cases

| # | Failure | How the design handles it |
|---|---|---|
| 1 | Command not on the allowlist (any other mode including 0x04, an `ST` command, a malformed string) | Refused before any write; counted; 0 bytes reach the transport |
| 2 | `NO DATA`, `?`, `CAN ERROR`, `BUFFER FULL`, `STOPPED`, `UNABLE TO CONNECT`, `BUS INIT: ...ERROR`, `ERROR` | Classified by name; no data bytes can be extracted from them |
| 3 | Negative response `7F 01 12` | Kind NegativeResponse; no data |
| 4 | Truncated reply (`410D` with no byte) | Kind Data, but `mode01DataBytes` returns an empty byte vector and `decodePid` refuses the count; the source counts malformed input |
| 5 | Non-hexadecimal text where data was expected (`41 0D ZZ`) | Kind Malformed; counted |
| 6 | No prompt within the timeout | Kind Timeout; counted; whatever was read is kept in `rawText` for logs |
| 7 | Transport closed or write fails | Kind LinkError; counted; the source raises LinkLost on its state machine (LHU-012) |
| 8 | Reply without a `>` but longer than 4,096 bytes | Malformed; reading stops |
| 9 | Reply arrives in many small chunks | Accumulated until the prompt; the fake's chunk size tests it |
| 10 | Random bytes of any length | Never a crash, never a sanitizer report, never data bytes unless the line is well formed (the 100,000-string test of REQ-010) |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | `CommandAllowlistTest`: all 256 mode values as `XX0D` (and `XX` alone) through `Elm327Protocol::execute` on a fake transport: only `01`, `03`, `09` are written; 0 bytes written for the other 253; `04` named and asserted refused and unwritten; each setup command allowed; `ATSH`, `ATPB`, `STI`, `01`, `010`, `01 0D`, `010D1`, lower case variants handled as specified | REQ-001 |
| 2 | `CommandAllowlistTest`: `obdRequest` builds `010D`, `03`, `0902` and refuses 0x04 and every other mode | REQ-001 |
| 3 | `Elm327ProtocolTest`: the main scenario; echo and `SEARCHING...` dropped; spaces on and off; two-line multi-module reply takes the first; AT replies classified Ok or Text; chunked delivery; counters | REQ-010 |
| 4 | `Elm327ProtocolMalformedInputTest`: the named corpus of REQ-010, each giving its kind and no data bytes; timeout; link error; oversize reply; 100,000 random byte strings (fixed seed) through the parser under the sanitizers preset: 0 crashes, data bytes only for well-formed lines | REQ-010 |
| 5 | `FakeByteTransportTest`: scripted replies, chunking, closed transport, recorded writes | fixture |

## 9. Alternatives considered

1. **Allowlist by mode prefix only (`01`, `03`, `09`).** Simpler. Rejected: `01ZZ` or `010D04` would pass a prefix check and reach the adapter; the exact grammar makes the 256-mode test meaningful and refuses malformed strings before they are sent.
2. **Allowlist as data in a configuration file.** Rejected: the whole point is that no configuration can enable a write; the list is code, reviewed, and the test pins it.
3. **A line-oriented transport (`readLine`) instead of bytes.** Rejected: the prompt `>` has no line ending after it, so a line reader would block; reading bytes until `>` with a deadline matches the adapter's actual framing.
4. **Throwing on refused commands.** Rejected for the same reason as DN-007: a refusal is a counted, visible event on the worker thread, not a crash.
5. **Letting the protocol decode PIDs itself.** Rejected: decoding is DN-009's pure function and the protocol stays about text framing; the source composes the two.

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #52 (6396c89). One addition: `Elm327Protocol::execute` drains and counts bytes the adapter sent before the new command is written (`discardedStaleByteCount`), because a late reply arriving after a timeout would otherwise be read as the answer to the next command; found while writing the chunked-reply test. One clarification: `Text` is a kind only for `AT` replies; printable text where data was expected is Malformed. Review finding carried to LHU-012: the real transports must honour a zero-millisecond read timeout, or the drain would block. Status: Implemented.
