# Code review checklist

Every pull request gets a written review against this checklist, posted as a PR comment before merge. The reviewer copies the template at the bottom, fills in every row, and lists findings with file and line.

This repository has one human contributor, so GitHub's required approvals are switched off (an author cannot approve their own PR). The gate is this written review plus the required CI status checks.

## How to answer each row

- **Pass**: checked, no problem found. Say what was checked.
- **Finding**: a problem. Give the file, the line, what is wrong and what would fix it. Mark it **Must fix** (blocks merge) or **Should fix** (can become a ticket).
- **Not applicable**: say why in a few words. "Not applicable" with no reason is not accepted.

## The checks

### 1. Correctness
- Does the code do what the ticket and the referenced requirements say?
- Are boundary values handled: empty input, zero, maximum, negative, first and last element?
- Are units and scaling right, and stated in names where a mistake is possible?

### 2. Error handling
- Is every failure of an external input (adapter, transport, file, operating system call) detected and handled?
- Does bad input ever produce a value marked Valid? It must not (REQ-010).
- Are errors reported to the caller or counted, never silently dropped?

### 3. Tests added
- Does every non-UI component touched have unit tests, including failure cases?
- Would each new test fail if the code under test were broken?
- Are tests tagged with the requirement IDs they verify?

### 4. Naming
- Are names literal and descriptive? No single-letter variable names, even in loops.
- Do names say what a thing is, with its unit where relevant?

### 5. Memory and ownership
- Is the owner of every object clear from the types (value, `std::unique_ptr`, reference, Qt parent)?
- No raw owning pointers, no manual `new` and `delete` without a stated reason.
- No reference or pointer that can outlive what it points to.

### 6. Thread safety
- Which thread runs this code, and is that stated where it matters?
- Is data shared between threads protected, or passed by a queued signal?
- Does anything touch a Qt object from a thread other than the one that owns it?

### 7. Performance
- Any work in a per-sample or per-frame path that could be done once?
- Any allocation, copy or blocking call in a hot path or on the UI thread?
- If the PR makes a performance claim, is the measurement attached with its sample size?

### 8. Requirement IDs referenced
- Does the PR body list the requirement IDs?
- Is the traceability matrix updated for every requirement, design element or test that changed?

### 9. Safety and privacy
- Does anything send data toward the vehicle outside the allowlist (REQ-001)?
- Any VIN, Bluetooth address, raw on-car recording, personal information or credential in the diff?

## Review comment template

```markdown
## Written review — LHU-nnn

Reviewed commit: <short hash>
Reviewer: <name>

| # | Check | Result | Notes |
|---|---|---|---|
| 1 | Correctness | Pass / Finding / Not applicable | |
| 2 | Error handling | | |
| 3 | Tests added | | |
| 4 | Naming | | |
| 5 | Memory and ownership | | |
| 6 | Thread safety | | |
| 7 | Performance | | |
| 8 | Requirement IDs referenced | | |
| 9 | Safety and privacy | | |

### Findings

1. **Must fix** — `path/to/file.cpp:42` — what is wrong; what would fix it.
2. **Should fix** — ...

### Verdict

Approve for merge / Changes required.
CI status at review time: <green or red, with the failing check>.
```
