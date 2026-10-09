# DN-031: Trip analytics as derived signals

| | |
|---|---|
| Ticket | LHU-031 |
| Requirements | REQ-022 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-09, about 50 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-09 |

## 1. Problem

The head unit shows what the car reports, but not what a driver asks about a trip: how much fuel it is using now and on average, how far it has gone, how long the engine spent in each speed range, and how long it took to warm up. This ticket computes those values in the service layer from the live samples, publishes them as ordinary signals so every consumer (store, D-Bus, view models) gets them without new plumbing, and shows them on a trip screen (REQ-022).

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | Which inputs? | Vehicle speed (PID 0x0D), engine speed (0x0C), coolant temperature (0x05), and mass air flow (0x10), which becomes a ninth measured signal. Whether the GS350 supports 0x10 is open (OQ-32); without it the two fuel-economy signals are never received and everything else works |
| 2 | Fuel economy formula | Fuel mass flow = air mass flow / air-fuel ratio; fuel volume flow = fuel mass flow / fuel density; litres per 100 km = litres per hour / (km/h) x 100. Constants: **air-fuel ratio 14.7** (stoichiometric petrol, the ratio the engine controller targets in closed loop; E10 would be about 14.1, a 4 % difference), **fuel density 745 g/L** (petrol at 15 °C; the range is about 720 to 775 g/L). Both are stated constants, not measured on this car. Check value: 10 g/s at 100 km/h gives 10 / 14.7 x 3600 / 745 = 3.2872 L/h, which is 3.2872 L/100 km |
| 3 | Instantaneous economy at standstill? | Litres per 100 km is undefined at 0 km/h. It is computed only when the latest speed is at least **5 km/h** and no older than **2,000 ms** at the air-flow sample's time; otherwise no sample is emitted, so the tile goes Stale after its timeout. That is the honest display: no number rather than an infinite one |
| 4 | Trip distance and fuel used | Trapezoidal integration between consecutive accepted samples of the same input: distance from speed, fuel from the fuel volume flow. A gap longer than **5,000 ms** (link lost, adapter asleep) is not integrated and is counted; integrating across it would invent distance |
| 5 | Trip-average economy | Fuel used / distance x 100, emitted with each air-flow sample once the trip distance is at least **0.1 km**, so the first metres do not show absurd values |
| 6 | RPM bands | Four bands: below 1,000; 1,000 to 2,499; 2,500 to 3,999; 4,000 and above. The time between two engine-speed samples is added to the band of the earlier sample. All four totals are emitted on each engine-speed sample, so each is Valid from the first one, at 0 if unused |
| 7 | Coolant warm-up time | The time from the trip's first coolant sample to its first sample at or above **80 °C**. Nothing is emitted until then (the tile shows no value, never received); afterwards the fixed value is emitted with each coolant sample. A trip that starts warm shows 0.0 |
| 8 | What is a trip? | It starts with the first input sample after the service starts, and starts again when no input sample has arrived for **10 minutes** (ignition off, car parked). A link drop shorter than that does not split a trip |
| 9 | Which clock? | Only the timestamps of the samples. No wall clock is read, so replaying a recording gives the same totals every time (REQ-022's replay criterion, and REQ-015) |
| 10 | Where does it run? | `DerivedSignalEngine`, plain C++17 in the service library (D-008). The `SignalStoreFeeder` owns one: after it accepts a measured sample it passes it to the engine and accepts the derived samples the same way, so they reach the store, the staleness monitor and the D-Bus hook like any source's samples. Derived samples are never fed back into the engine |
| 11 | Timestamps of derived samples | The timestamp of the input sample that produced them. Each derived signal has exactly one triggering input, so its timestamps rise whenever the input's do and the store never rejects one as out of order |
| 12 | How are they shown? | A trip screen: the same 4 x 2 grid and tiles as the vehicle-data screen (DN-039) with the eight derived signals, opened from a Trip button on Home. Fuel economy, distance and minutes show one decimal |
| 13 | Does the enumeration change break anything? | New `SignalId` values are appended, so the numbers on D-Bus for the first eight stay the same. `signalCount` and the new `unitCount` are checked against the last enumerator by `static_assert`, which closes the review finding about the literal. Lists replace "all signals" where a component means a subset: `measuredSignalIds` (what a source can produce), `gridSignalIds` (REQ-004's eight tiles), `derivedSignalIds` (this ticket's eight) |

## 3. Nouns to classes

| Class | Library | Responsibility |
|---|---|---|
| `SignalId` (extended), `Unit` (extended), `measuredSignalIds`, `gridSignalIds`, `derivedSignalIds` | service | Mass air flow and the eight derived signals; grams per second, litres per 100 km, kilometres, minutes |
| `DerivedSignalConstants` | service | The stated constants of section 2, in one place, with their units in the names |
| `DerivedSignalEngine` | service | Trip state and the formulas; `onSample(sample) -> derived samples` |
| `SignalStoreFeeder` (changed) | service | Owns the engine; accepts derived samples like measured ones; counts them separately |
| `decodePid` and the PID table (changed) | OBD | PID 0x10, mass air flow, (256 x A + B) / 100 g/s |
| `VehicleDataViewModel` (changed) | HMI | `tiles` stays the grid's eight; `tripTiles` the eight derived |
| `VehicleDataScreen.qml` (changed), `Screens.qml`, `HomeScreen.qml` | HMI | The grid takes its tile list as a property; the trip screen is a second instance |

## 4. What each class stores

- `DerivedSignalEngine`: the constants; the trip start time; per input the last accepted sample (value and time); distance in km, fuel in litres, four band totals in milliseconds; the warm-up start time and, once reached, the warm-up time; the count of gaps not integrated and the count of trips started.
- `SignalStoreFeeder`: the engine by value and a derived-sample count.

## 5. Verbs to methods

| Class | Method | Notes |
|---|---|---|
| `DerivedSignalEngine` | `onSample(const SignalSample&) -> std::vector<SignalSample>` | Ignores derived and non-Valid samples; returns 0 to 4 samples |
| `DerivedSignalEngine` | `tripDistanceKilometres()`, `tripFuelLitres()`, `gapsNotIntegrated()`, `tripsStarted()` | For tests and the session log |
| free | `instantFuelEconomy(massAirFlowGramsPerSecond, speedKilometresPerHour, constants) -> double` | L/100 km; the formula of section 2, row 2 |
| free | `fuelLitresPerHour(massAirFlowGramsPerSecond, constants) -> double` | |
| `SignalStoreFeeder` | `derivedSampleCount()`, `derivedSignalEngine()` | |
| `VehicleDataViewModel` | `tripTiles` (property) | |

## 6. Interaction sequence

1. The source emits a mass-air-flow sample; the feeder accepts it into the store and calls its sample hook (D-Bus or the UI bridge).
2. The feeder passes the sample to the engine. The engine adds the fuel since the previous air-flow sample to the trip, and returns the instantaneous economy (if speed allows) and the trip average (if the distance allows), both with the air-flow sample's timestamp.
3. The feeder accepts each derived sample the same way: store, staleness, hook. On D-Bus they are ordinary `SampleChanged` messages; the client and the view model need no change beyond the trip tiles.
4. A speed sample adds distance and returns the trip distance; an engine-speed sample adds time to a band and returns the four band totals; a coolant sample returns the warm-up time once reached.

## 7. Failure cases

| Failure | Handling |
|---|---|
| PID 0x10 unsupported | No air-flow samples, so both economy signals are never received; the other six are computed |
| Speed missing or stale at an air-flow sample | No instantaneous economy for that sample; the trip fuel is still added |
| Gap over 5 s in an input | Not integrated; counted in `gapsNotIntegrated` |
| No input for 10 minutes | A new trip: totals to zero, warm-up measured again |
| Stale input sample (status not Valid) | Ignored by the engine |
| A sample older than the input's last | Never reaches the engine: the store rejects it first |
| Replay of the same recording twice | Identical totals, because only sample timestamps are used |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | Formula: 10 g/s at 100 km/h equals 3.2872 L/100 km within 0.1 %; L/h at 10 g/s | REQ-022 |
| 2 | Trip distance from a synthetic constant 50 km/h trace over 600 s every 500 ms equals 8.333 km within 0.1 % | REQ-022 |
| 3 | Instantaneous economy only at or above 5 km/h with a speed no older than 2 s; trip average only from 0.1 km | REQ-022 |
| 4 | RPM bands: a trace through all four bands gives the hand-computed times; all four emitted on each sample | REQ-022 |
| 5 | Warm-up: nothing before 80 °C, then the time, then the same value; a warm start gives 0 | REQ-022 |
| 6 | Gaps over 5 s not integrated and counted; 10 minutes of silence starts a new trip | REQ-022 |
| 7 | Feeder: derived samples reach the store and the hook with the input's timestamp; measured counts unchanged | REQ-022 |
| 8 | Replay: a session recorded against the emulator, replayed twice through a feeder each time, gives identical final derived values, all non-zero | REQ-022, REQ-015 |
| 9 | Emulator: mass air flow polled and decoded; the poll list grows by one | REQ-022, REQ-004 |
| 10 | HMI: the trip screen shows the eight derived tiles with one decimal and a Home button; Home has a Trip button of at least 10 mm | REQ-022 |

## 9. Alternatives considered

| Alternative | Why rejected |
|---|---|
| Derived values in the view model | Only the in-process app would see them; the D-Bus consumers would each recompute them, differently |
| A separate `TripData` struct and D-Bus member | A second path for numbers that behave exactly like signals; REQ-022 asks for them to be exposed like any other signal |
| Fuel rate from PID 0x5E (engine fuel rate) | Many petrol cars of this age do not support it; air flow is the common input. If the car supports 0x5E it can be added as a second formula later |
| Wall-clock timing | Replays would not repeat exactly, and the clock of the Pi may be wrong at boot |
| Instantaneous economy as L/h at standstill in the same signal | One signal with two units; the trip screen can show L/h later as its own signal if wanted |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

| # | Design | Implementation | Reason |
|---|---|---|---|
| 1 | The feeder passes the accepted sample to the engine | It passes the stored copy (`SignalStore::latest`) | Found by the replay test: sources emit samples with the default status, and only the store marks them Valid. Given the incoming sample, the engine ignored everything and produced no derived signal at all |
| 2 | Not foreseen | Home's buttons are placed by bindings on `x`, not by a `Row` | A `Row` positions its children on the next polish. In the first frame the Trip button lay at x = 0 on top of Vehicle data and took its tap; the existing Screens test caught it |
| 3 | The poll list grows by one | Mass air flow is polled last, after fuel level | The first eight keep their order, so the existing order tests still describe the grid; each PID is now polled every ninth request instead of every eighth (A6 still decides the real rate) |
| 4 | Not foreseen | The service-executable test checks the measured signals and three derived ones, not all seventeen | The trip average needs 0.1 km and the warm-up 30 s of demo driving, longer than the test's 5 s; both are covered by the unit tests |
| 5 | `pidForSignal` returned a PID for every signal | It returns an optional; derived signals have none | A derived signal must never become a request to the car |

Not verified: whether the GS350 supports PID 0x10 (OQ-32), and how far the computed distance and average economy are from the car's own trip meter. That is checklist step 3.11 under assumption A16.
