## Ticket

LHU-nnn: <ticket title>

## Requirements

<!-- List every requirement this PR implements or verifies, for example REQ-003, REQ-006.
     Write "None: process or documentation only" if no requirement is touched. -->

## Design

<!-- Keep the sentence below and fill in the design note number.
     Write "Exempt: documentation, build or CI only" if the ticket has no design note. -->

Implements DN-nnn; deviations from the design are listed with reasons.

- Deviations: <!-- "None", or each deviation with its reason -->
- [ ] The design note is marked Approved and is in this PR or already on `dev`

## What changed and why

<!-- A few sentences. Say what the reader should look at first. -->

## How it was verified

<!-- Commands run and their results. Paste numbers, not adjectives. -->

- [ ] CI is green (build, unit tests, static analysis)
- [ ] Unit tests added or updated for every non-UI component touched
- [ ] `docs/traceability/TRACEABILITY.md` updated, or not affected
- [ ] Any performance or correctness claim has a measurement with sample size and raw data attached

## Safety and privacy

- [ ] Nothing here sends anything to the vehicle beyond the allowlisted OBD-II read requests (REQ-001)
- [ ] No VIN, no Bluetooth address, no raw on-car recording
- [ ] No personal information, credentials or third-party names

## Review

The written review against `docs/review/CODE_REVIEW_CHECKLIST.md` is posted as a comment on this PR before merge. Required approvals are off because GitHub does not let an author approve their own PR; the checklist comment and the required status checks are the gate.

- [ ] Review checklist comment posted
- [ ] Every "Must fix" finding resolved
