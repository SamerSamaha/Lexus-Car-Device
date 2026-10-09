# Design notes

Every ticket that adds or changes a component gets a written design note, reviewed and approved **before** any code is written. This is the design-first gate.

## Why

- A design mistake costs minutes to fix in a note and hours to fix in code.
- The note makes the author state the interfaces, the units, the failure cases and the tests up front, which is what a code review then checks the code against.
- The design is defended in a review before it is built, so the reasons behind it are known and written down.

## Which tickets

The gate applies to every ticket that adds or changes a component: a class, an interface, a data source, a view model, a screen, or a tool with its own logic. A ticket's status does not move to **In progress** until its note is marked **Approved**.

Tickets that only change documentation, the build, or CI are exempt.

## The process

| Step | What happens | Time box |
|---|---|---|
| 1. Draft | The author writes the note alone, from `DESIGN_NOTE_TEMPLATE.md`, before discussing any solution. Saved as `docs/design/DN-nnn-<short-name>.md`, where `nnn` is the ticket number. Status: Draft | 30 to 45 minutes |
| 2. Design review | A senior reviewer questions the design: one or two questions at a time, pointing at gaps by asking, not by telling. The reviewer does not show a design of their own until the author has defended or revised theirs. Status: In review | About 30 minutes |
| 3. Comparison | The reviewer shows how they would have designed it, and the key differences | About 15 minutes |
| 4. Revision | The author revises the note and records what changed in "Changes after the design review" | Part of step 3 |
| 5. Approval | The author marks the note Approved. Coding starts only now | |
| 6. Commit | The note is committed with the code pull request, or before it | |
| 7. After merge | The author adds the "Design vs. implementation" section: where the code differs from the note, and why. Status: Implemented | About 15 minutes |

## Build-out form (D-049)

For the build-out from v0.1.0 to v1.0.0 the note is written by the implementer of the ticket, in the same form and with the same sections, marked Approved, and committed in the pull request that implements it. The repository owner reviews the note and the code together after merge and adds the "Design vs. implementation" section. Assumptions made in place of a hardware fact are listed in section 2 of the note and in the assumptions table of `docs/release/PI_BRINGUP_CHECKLIST.md`.

## Rules

- The draft is time-boxed. When the time is up, the draft goes to review as it is. An unfinished section is a finding for the review, not a reason to keep writing.
- The pull request that implements a note says so: "Implements DN-nnn; deviations from the design are listed with reasons."
- A note is a record. After approval it is changed only through the "Changes after the design review" and "Design vs. implementation" sections, so the original design and what was learned both stay visible.
- A design note describes a design. It makes no claim of compliance with any standard.

## Sections of a note

In this order:

1. Problem, in two sentences.
2. Clarifying questions, with the assumption made for each.
3. Nouns to classes, with each class's responsibility.
4. What each class stores: constructor and fields.
5. Verbs to methods: the public interface with inputs, outputs and units.
6. Interaction sequence for the main scenario.
7. Failure cases and how the design handles each.
8. Test plan mapped to requirement IDs.
9. At least one alternative considered, and why it was rejected.

## Index

| Note | Ticket | Status |
|---|---|---|
| [DN-006](DN-006-signal-store.md) Signal model and SignalStore with staleness | LHU-006 | Implemented |
| [DN-007](DN-007-connection-state-machine.md) Connection state machine | LHU-007 | Implemented |
| [DN-008](DN-008-vehicle-data-source.md) VehicleDataSource interface and FakeSource | LHU-008 | Implemented |
| [DN-009](DN-009-obd-pid-decoder.md) OBD PID decoder | LHU-009 | Implemented |
| [DN-010](DN-010-elm327-protocol.md) ELM327 response parser and command allowlist | LHU-010 | Implemented |
| [DN-011](DN-011-elm327-emulator.md) ELM327 emulator with fault injection | LHU-011 | Implemented |
| [DN-012](DN-012-elm327-obd-source.md) Elm327ObdSource | LHU-012 | Implemented |
| [DN-013](DN-013-qml-home-screen.md) QML home screen, view models and the application | LHU-013 | Implemented |
| [DN-039](DN-039-vehicle-data-screen.md) Vehicle-data screen, 4 x 2 signal grid | LHU-039 | Implemented |
| [DN-015](DN-015-thermal-logger.md) Thermal and power logger | LHU-015 | Implemented |
| [DN-021](DN-021-app-hub.md) App hub launcher | LHU-021 | Implemented |
| [DN-022](DN-022-vehicle-data-service.md) Vehicle-data service over D-Bus | LHU-022 | Implemented |
| [DN-027](DN-027-dbc-decoder.md) DBC decoder with an independent oracle | LHU-027 | Implemented |
