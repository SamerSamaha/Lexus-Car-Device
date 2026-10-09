# DN-027: DBC decoder with an independent oracle

| | |
|---|---|
| Ticket | LHU-027 |
| Requirements | REQ-005, REQ-010 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-09, about 35 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-09 |

## 1. Problem

The CAN path (LHU-028) needs a decoder that turns a raw CAN frame into physical signal values using a DBC file, for both byte orders, signed and unsigned values, scale and offset (REQ-005). Its correctness must be shown against an independent implementation, the Python `cantools` library, on at least 10,000 random frames, and a malformed frame must be a counted error, never a sample (REQ-010).

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | Which DBC? | `dbc/simulated_vehicle.dbc`, an invented vehicle, labelled as such in its comments. It is not the GS350: real frames of the car are out of scope. Its signals are chosen for coverage: little and big endian, signed and unsigned, lengths 1 to 32 bits, signals crossing byte boundaries, fractional and negative scales and offsets, a 4-byte message beside 8-byte ones, and the eight project signals for LHU-028 |
| 2 | How much of the DBC grammar? | `BO_` and `SG_` lines (message id, name, length, sender; signal name, start bit, length, byte order, sign, scale, offset, minimum, maximum, unit, receivers). Every other keyword (`VERSION`, `NS_`, `BS_`, `BU_`, `CM_`, `BA_`, `VAL_` and so on) is skipped. Multiplexed signals and signal-value types (float signals) are not supported: such a line is a parse error, not silently wrong. An identifier with bit 31 set is an extended (29-bit) identifier |
| 3 | How are bits numbered? | As in DBC files and `cantools`: bit `n` is bit `n % 8` of byte `n / 8`, counting from the least significant bit. A little-endian (`@1`) signal's start bit is its least significant bit; a big-endian (`@0`) signal's start bit is its most significant bit, and the next lower bit after bit 0 of a byte is bit 7 of the next byte |
| 4 | What does "wrong length" mean? | The frame's data length differs from the message length in the DBC, either way. `cantools` decodes strictly by default and so does this decoder. A length above 8 is an invalid classic CAN frame. Unknown identifiers are counted separately; on a real bus they are normal (other ECUs' traffic) |
| 5 | How is `cantools` used without making it a dependency of the product? | Only in a test. `tools/dbc_oracle.py` loads the DBC with `cantools`, generates frames from a fixed seed (random message, random bytes of the right length), decodes them, and writes one line per decoded signal. The C++ test decodes the same frames and compares every value. `cantools` lives in a test-only virtual environment (`tools/setup_test_venv.sh`, `tools/requirements/test.txt`, approved 2026-10-08), never system-wide and never on the Pi |
| 6 | What tolerance? | 1e-6 absolute after scaling (REQ-005). Both sides compute `raw * scale + offset` in double precision on raw values of at most 32 bits, so they should agree exactly; the tolerance is there for printing and parsing |
| 7 | What does the test print? | Every mismatch in full (frame number, identifier, data bytes, signal, both values, difference), never a count alone (REQ-005) |
| 8 | Which layer? | `src/hardware/can/`, library `lexus_head_unit_can`, plain C++17, no Qt, no socket. The SocketCAN reader and the source come in LHU-028; this ticket is the pure decoding |

## 3. Nouns to classes

| Class | Responsibility |
|---|---|
| `CanFrame` (struct) | Identifier, extended flag, data length, 8 data bytes, timestamp |
| `DbcSignal`, `DbcMessage` (structs) | One signal's layout and scaling; one message's identifier, length and signals |
| `DbcDatabase` | Parses DBC text into messages; looks up a message by identifier; collects parse errors with line numbers |
| `DbcDecoder` | Decodes a frame against the database: counted result kinds, the physical value of every signal |
| `extractRawValue` (free function) | The bit extraction for both byte orders, with sign extension |

## 4. What each class stores

- `DbcSignal`: `std::string name, unit`; `std::uint32_t startBit, length`; `ByteOrder byteOrder`; `bool isSigned`; `double scale, offset, minimum, maximum`.
- `DbcMessage`: `std::uint32_t identifier`; `bool extended`; `std::string name, sender`; `std::uint32_t length`; `std::vector<DbcSignal> signalList`.
- `DbcDatabase`: `std::vector<DbcMessage>`, a map from (identifier, extended) to the message index, `std::vector<std::string> errors`.
- `DbcDecoder`: the database, counters (decoded, unknown identifier, wrong length, invalid frame).

## 5. Verbs to methods

| Class | Method | Inputs | Output |
|---|---|---|---|
| `DbcDatabase` | `parse(text)` (static) | DBC text | database; `errors()` empty when it parsed cleanly |
| | `loadFromFile(path)` (static) | path | database, or an error "cannot open" |
| | `messages()`, `find(identifier, extended)`, `errors()` | | |
| free | `extractRawValue(data, signal)` | 8 bytes, signal | `std::int64_t` (sign-extended when signed) |
| free | `physicalValue(signal, raw)` | | `raw * scale + offset` |
| `DbcDecoder` | `decode(frame)` | `CanFrame` | `DecodeResult`: kind (Decoded, UnknownIdentifier, WrongLength, InvalidFrame), the message name, a list of (signal name, value, unit) |
| | `counters()` | | the four counts |

## 6. Interaction sequence

1. LHU-028's source loads `dbc/simulated_vehicle.dbc` once with `DbcDatabase::loadFromFile`; a non-empty `errors()` stops the source from starting.
2. For each frame from the reader, `decode(frame)`: invalid length above 8 gives InvalidFrame; no message for the identifier gives UnknownIdentifier; a length different from the message's gives WrongLength; otherwise every signal is extracted and scaled.
3. The source maps the names it knows to `SignalId` and publishes samples (LHU-028).

## 7. Failure cases

| Failure | Handling |
|---|---|
| Signal extends past the message length | Parse error naming the line |
| Multiplexed signal, float signal, signal length 0 or above 64 | Parse error naming the line |
| Duplicate message identifier | Parse error |
| Malformed `BO_` or `SG_` line | Parse error with the line number; parsing continues so all errors are reported at once |
| Frame length above 8 | InvalidFrame, counted |
| Unknown identifier | UnknownIdentifier, counted |
| Length differs from the DBC | WrongLength, counted, no values |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | Hand-computed vectors: little-endian across bytes, big-endian across bytes, signed minimum and maximum, 1-bit and 32-bit signals, scale and offset | REQ-005 |
| 2 | Oracle: 10,000 frames from a fixed seed decoded by `cantools` and by `DbcDecoder`; every value within 1e-6; every DBC signal appears in the oracle output; every mismatch printed in full | REQ-005 |
| 3 | Parser: the committed DBC parses with no errors and the expected messages and signals; each failure case of section 7 gives its error | REQ-005, REQ-010 |
| 4 | Malformed frames: a named corpus (empty, short, long, length 9 to 15, unknown identifier, extended flag mismatch) and 100,000 random frames under sanitizers; counters add up; no value from a frame whose length is wrong | REQ-010 |

## 9. Alternatives considered

| Alternative | Why rejected |
|---|---|
| Committing the oracle's output as a golden file | Would freeze one run of `cantools`; a regenerated run with the pinned version is the independent check, and the seed makes it repeatable |
| A third-party C++ DBC library | Hides exactly what REQ-005 asks to show (bit layout for both byte orders) and adds a dependency to the Pi build |
| Writing the oracle by hand in Python | Not independent: the same person writing both implementations repeats the same misunderstanding |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #62. No deviation from the interface. Details the note did not spell out:

- Numbers in the DBC are parsed with `std::from_chars`, which ignores the process locale; the Qt applications set the locale from the environment, and a decimal comma would otherwise break `strtod`.
- `extractRawValue` walks the bits without allocating; `signalBitPositions` (which allocates) is used only when parsing, to check that every signal fits its message.
- The oracle runs as a CTest fixture: `DbcOracleGenerate` writes the CSV and the comparison test requires it, so the comparison never runs on a stale or missing file.

Evidence: 10,000 frames from seed 20261009, 42,558 signal values, all 17 signals, 0 mismatches against `cantools` 44.2.1. The oracle was checked by a planted defect (the big-endian step to the next byte changed from 15 to 7): the test failed and listed 9,962 mismatches in full; the defect was then removed. Status: Implemented.
