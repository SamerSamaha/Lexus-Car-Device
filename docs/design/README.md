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
| (none yet) | | |
